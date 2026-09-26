#ifndef _WIN32

#include "platform.h"
#include <SFML/Graphics.hpp>
#include <iostream>
#include <cstdio>


bool loadAppIcon(sf::Image& icon)
{
    if(icon.loadFromFile("../assets/mishicon.png"))
    {
        return true;
    }

    std::cerr << "Failed to load MISH icon\n";
    return false;
}



std::string openFolderDialog()
{
    std::string result;


    FILE* pipe = popen(
        "zenity --file-selection --directory",
        "r"
    );


    if(!pipe)
    {
        return "";
    }


    char buffer[512];


    if(fgets(buffer, sizeof(buffer), pipe))
    {
        result = buffer;


        if(!result.empty() && result.back() == '\n')
        {
            result.pop_back();
        }
    }


    pclose(pipe);


    return result;
}

#endif
