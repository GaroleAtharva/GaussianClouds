
#include <iostream>
#include <GLFW/glfw3.h>
// #include <glad/gl.h> // will be done tmrw

static void error_callback(int error, const char* description){

    std::cerr << "GLFW Error (" << error << "): " << description << std::endl;
   
}

static void key_callback(GLFWwindow* window,
                         int key,
                         int scancode,
                         int action,
                         int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}
static void framebuffer_size_callback(GLFWwindow* window,
                                      int width,
                                      int height)
{
    glViewport(0, 0, width, height);
}

int main(void){

    glfwSetErrorCallback(error_callback);

    if(!glfwInit()){ 
        glfwTerminate();
        return -1;
    } 

    // Creating the window and OpenGl context

    GLFWwindow* window = glfwCreateWindow(640,480, "Gaussian Rendering Test", NULL , NULL);
    if(!window){

        glfwTerminate();
        return -1;
    }
    //making the opengl's current conent to window
    glfwMakeContextCurrent(window);

    //Setting buffer resize
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    //seting a escape button key callback
    glfwSetKeyCallback(window, key_callback);


    //opengl framebuffer
    int height =0; int  width =0;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);   

    //write an event loop
    while(!glfwWindowShouldClose(window)){
        glfwPollEvents();
        
        
        //render parts
        glClearColor(0.1f, 1.0f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glBegin(GL_TRIANGLES);
            glColor3f(1.0f, 0.0f, 0.0f);   // red
            glVertex2f(-0.5f, -0.5f);

            glColor3f(0.0f, 1.0f, 0.0f);   // green
            glVertex2f( 0.5f, -0.5f);

            glColor3f(0.0f, 0.0f, 1.0f);   // blue
            glVertex2f( 0.0f,  0.5f);
        glEnd();


        glfwSwapBuffers(window); //swaping the front and back buffers
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}