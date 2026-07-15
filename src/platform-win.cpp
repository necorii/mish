#ifdef _WIN32

#include <windows.h>
#include <shobjidl.h>
#include "platform.h"

std::string openFolderDialog() {
    std::string folderPath = "";
    
    // Initialize COM library
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (SUCCEEDED(hr)) {
        IFileOpenDialog *pDlg = NULL;
        
        // Create the FileOpenDialog object
        hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, 
                             IID_IFileOpenDialog, reinterpret_cast<void**>(&pDlg));
        
        if (SUCCEEDED(hr)) {
            // Set options to select folders instead of files
            DWORD dwOptions;
            pDlg->GetOptions(&dwOptions);
            pDlg->SetOptions(dwOptions | FOS_PICKFOLDERS);
            
            // Show the dialog
            hr = pDlg->Show(NULL);
            
            if (SUCCEEDED(hr)) {
                IShellItem *pItem = NULL;
                hr = pDlg->GetResult(&pItem);
                
                if (SUCCEEDED(hr)) {
                    PWSTR pszPath = NULL;
                    hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);
                    
                    if (SUCCEEDED(hr)) {
                        // Convert Wide String (UTF-16) to UTF-8 std::string
                        int size_needed = WideCharToMultiByte(CP_UTF8, 0, pszPath, -1, NULL, 0, NULL, NULL);
                        if (size_needed > 0) {
                            folderPath.resize(size_needed - 1);
                            WideCharToMultiByte(CP_UTF8, 0, pszPath, -1, &folderPath[0], size_needed, NULL, NULL);
                        }
                        CoTaskMemFree(pszPath);
                    }
                    pItem->Release();
                }
            }
            pDlg->Release();
        }
        CoUninitialize();
    }
    return folderPath;
}

#endif