#include "Renderer.h"

int main()
{
    Renderer renderer;

    renderer.Setup();
    renderer.Render();
    renderer.Cleanup();

    return 0;
}