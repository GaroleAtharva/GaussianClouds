#include <iostream>
#include <GLFW/glfw3.h>

using namespace std;


int main(){
    if(!glfwInit()){ 
        glfwTerminate();
        return -1;
    } 

    GLFWwindow* window = glfwCreateWindow(640, 480, "Gaussian Splat", NULL, NULL);
    if(!window){
        cout << "window init faild";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    while (!glfwWindowShouldClose(window)){

    }


    
    return 0;
}
