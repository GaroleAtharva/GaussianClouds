"""Generate small test PLY files for lighting development."""
import numpy as np
import struct
import sys
import os

def write_ply(filename, positions, colors, scales, rotations, opacities):
    """Write a 3DGS-compatible PLY file."""
    n = len(positions)

    # SH DC coefficients from linear RGB: f_dc = (color - 0.5) / 0.2820947917738781
    C0 = 0.2820947917738781
    sh_dc = (colors - 0.5) / C0

    # Log scale (3DGS stores log of scale)
    log_scales = np.log(np.clip(scales, 1e-7, None))

    # Inverse sigmoid for opacity
    opacities_clipped = np.clip(opacities, 1e-6, 1.0 - 1e-6)
    logit_opacities = np.log(opacities_clipped / (1.0 - opacities_clipped))

    header = f"""ply
format binary_little_endian 1.0
element vertex {n}
property float x
property float y
property float z
property float nx
property float ny
property float nz
property float f_dc_0
property float f_dc_1
property float f_dc_2
property float opacity
property float scale_0
property float scale_1
property float scale_2
property float rot_0
property float rot_1
property float rot_2
property float rot_3
end_header
"""

    with open(filename, 'wb') as f:
        f.write(header.encode('ascii'))
        for i in range(n):
            # position
            f.write(struct.pack('<fff', *positions[i]))
            # normals (placeholder)
            f.write(struct.pack('<fff', 0.0, 0.0, 0.0))
            # SH DC (base color)
            f.write(struct.pack('<fff', *sh_dc[i]))
            # opacity (logit)
            f.write(struct.pack('<f', logit_opacities[i]))
            # scale (log)
            f.write(struct.pack('<fff', *log_scales[i]))
            # rotation (quaternion wxyz)
            f.write(struct.pack('<ffff', *rotations[i]))

    size_kb = os.path.getsize(filename) / 1024
    print(f"Written {filename}: {n} gaussians, {size_kb:.1f} KB")


def generate_sphere(n=5000, radius=1.0):
    """Sphere of Gaussians — good for testing directional lighting."""
    # Points on sphere surface
    phi = np.random.uniform(0, 2 * np.pi, n)
    cos_theta = np.random.uniform(-1, 1, n)
    sin_theta = np.sqrt(1 - cos_theta**2)

    positions = np.column_stack([
        radius * sin_theta * np.cos(phi),
        radius * sin_theta * np.sin(phi),
        radius * cos_theta
    ])

    # Add slight jitter inward/outward for volume
    jitter = np.random.normal(0, 0.02, n)
    normals = positions / radius
    positions += normals * jitter[:, None]

    # White/light gray color
    colors = np.full((n, 3), 0.85) + np.random.uniform(-0.05, 0.05, (n, 3))
    colors = np.clip(colors, 0, 1)

    # Small flat splats oriented along the surface
    scales = np.column_stack([
        np.full(n, 0.04),  # tangent
        np.full(n, 0.04),  # tangent
        np.full(n, 0.008), # normal (thin)
    ])

    # Rotation: align smallest axis with surface normal
    rotations = np.zeros((n, 4))
    rotations[:, 0] = 1.0  # identity quaternion (will be approximate)

    # Compute quaternion to rotate Z-axis to normal direction
    for i in range(n):
        normal = normals[i]
        normal = normal / np.linalg.norm(normal)
        # Rotate from (0,0,1) to normal
        z = np.array([0, 0, 1])
        if np.allclose(normal, z):
            rotations[i] = [1, 0, 0, 0]
        elif np.allclose(normal, -z):
            rotations[i] = [0, 1, 0, 0]
        else:
            axis = np.cross(z, normal)
            axis = axis / np.linalg.norm(axis)
            angle = np.arccos(np.clip(np.dot(z, normal), -1, 1))
            half = angle / 2
            rotations[i] = [np.cos(half), *(axis * np.sin(half))]

    opacities = np.full(n, 0.8)
    return positions, colors, scales, rotations, opacities


def generate_cloud_blob(n=3000):
    """Cloud-like blob — good for testing subsurface scattering."""
    # Elongated ellipsoid with noise
    positions = np.random.normal(0, 1, (n, 3))
    positions[:, 0] *= 1.5  # stretch X
    positions[:, 1] *= 0.6  # squash Y
    positions[:, 2] *= 0.8

    # Cull points too far from center (make it blobby)
    dist = np.linalg.norm(positions, axis=1)
    mask = dist < 2.0
    positions = positions[mask]
    n = len(positions)

    # White cloud color with slight variation
    colors = np.full((n, 3), 0.9) + np.random.uniform(-0.03, 0.03, (n, 3))
    colors = np.clip(colors, 0, 1)

    # Larger, rounder splats for cloud volume
    base_scale = 0.08
    scales = np.column_stack([
        np.full(n, base_scale) * np.random.uniform(0.8, 1.2, n),
        np.full(n, base_scale) * np.random.uniform(0.8, 1.2, n),
        np.full(n, base_scale * 0.5) * np.random.uniform(0.8, 1.2, n),
    ])

    # Random rotations
    rotations = np.random.normal(0, 1, (n, 4))
    rotations = rotations / np.linalg.norm(rotations, axis=1, keepdims=True)
    # Ensure w > 0 for consistency
    rotations[rotations[:, 0] < 0] *= -1

    # Varying opacity — denser in center
    dist = np.linalg.norm(positions, axis=1)
    opacities = np.clip(0.9 - dist * 0.2, 0.3, 0.95)

    return positions, colors, scales, rotations, opacities


def generate_plane(n=2000):
    """Flat plane of Gaussians — good for verifying normal extraction."""
    x = np.random.uniform(-2, 2, n)
    z = np.random.uniform(-2, 2, n)
    y = np.random.normal(0, 0.01, n)  # nearly flat
    positions = np.column_stack([x, y, z])

    # Checkerboard-ish color
    checker = ((np.floor(x * 2) + np.floor(z * 2)) % 2).astype(float)
    colors = np.column_stack([
        0.3 + 0.5 * checker,
        0.3 + 0.5 * checker,
        0.4 + 0.4 * checker
    ])

    scales = np.column_stack([
        np.full(n, 0.05),
        np.full(n, 0.005),  # thin in Y (normal direction)
        np.full(n, 0.05),
    ])

    # Identity rotation (Y-up normal, but smallest scale is Y)
    # Need to rotate so smallest axis (Y here) maps correctly
    rotations = np.zeros((n, 4))
    rotations[:, 0] = 1.0  # identity

    opacities = np.full(n, 0.9)
    return positions, colors, scales, rotations, opacities


if __name__ == "__main__":
    out_dir = os.path.dirname(os.path.abspath(__file__))
    out_dir = os.path.join(os.path.dirname(out_dir))  # project root

    scenes = {
        "sphere": generate_sphere,
        "cloud": generate_cloud_blob,
        "plane": generate_plane,
    }

    which = sys.argv[1] if len(sys.argv) > 1 else "all"

    if which == "all":
        for name, gen_fn in scenes.items():
            data = gen_fn()
            write_ply(os.path.join(out_dir, f"test_{name}.ply"), *data)
    elif which in scenes:
        data = scenes[which]()
        write_ply(os.path.join(out_dir, f"test_{which}.ply"), *data)
    else:
        print(f"Unknown scene: {which}. Choose from: {', '.join(scenes.keys())}, all")
        sys.exit(1)
