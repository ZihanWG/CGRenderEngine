#include "Engine/Runtime/Application.h"

#include <exception>
#include <iostream>

int main()
{
    try
    {
        ApplicationOptions options;
        options.editorMode = true;
        Application editor(1440, 900, "CGEngine Editor", options);
        editor.Run();
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Editor fatal error: " << exception.what() << std::endl;
        return 1;
    }

    return 0;
}
