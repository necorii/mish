#ifdef _WIN32

#include "platform.h"
#include <windows.h>
#include <shobjidl.h>
#include <windows.h>
#include <SFML/Graphics.hpp>


std::string openFolderDialog()
{
    std::string result;

    IFileDialog* dialog = nullptr;

    HRESULT hr = CoCreateInstance(
        CLSID_FileOpenDialog,
        NULL,
        CLSCTX_ALL,
        IID_PPV_ARGS(&dialog)
    );


    if (SUCCEEDED(hr))
    {
        DWORD options;

        dialog->GetOptions(&options);

        dialog->SetOptions(
            options | FOS_PICKFOLDERS
        );


        if (SUCCEEDED(dialog->Show(NULL)))
        {
            IShellItem* item;

            if (SUCCEEDED(dialog->GetResult(&item)))
            {
                PWSTR path;

                item->GetDisplayName(
                    SIGDN_FILESYSPATH,
                    &path
                );


                char buffer[MAX_PATH];

                WideCharToMultiByte(
                    CP_UTF8,
                    0,
                    path,
                    -1,
                    buffer,
                    MAX_PATH,
                    nullptr,
                    nullptr
                );


                result = buffer;

                CoTaskMemFree(path);
                item->Release();
            }
        }


        dialog->Release();
    }


    return result;
}


bool loadAppIcon(sf::Image& icon)
{
    if (icon.loadFromFile("mishicon.png"))
    {
        return true;
    }

    return false;
}
#endif