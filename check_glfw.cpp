#include <iostream>
#include <GLFW/glfw3.h>


using namespace std;

int main(){
if(!glfwInit()){
    cout<< "Init failed";
} else {
    cout << "Init successfull";
}

glfwTerminate();
return 0;
}