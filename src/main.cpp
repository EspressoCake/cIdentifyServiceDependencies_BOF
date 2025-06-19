#include "headers/beacon.h"
#include "headers/win32.h"
#include "headers/utils.h"
#include <wbemidl.h>

extern "C" void dumpFormatAllocation(formatp* formatAllocationData, const wchar_t* service)
{
    char*   outputString = NULL;
    int     sizeOfObject = 0;
    int     newlines = 0;

    outputString = BeaconFormatToString(formatAllocationData, &sizeOfObject);
    COUNT_NEWLINES(outputString, newlines);

    if (newlines)
    {
        BeaconPrintf(CALLBACK_OUTPUT, "Identified %03d services dependent upon %S\n", newlines, service);
        BeaconOutput(CALLBACK_OUTPUT, outputString, sizeOfObject);
    }
    else
    {
        BeaconPrintf(CALLBACK_OUTPUT, "No dependencies found. Please double-check the spelling to ensure this is correct.");
    }
    
    BeaconFormatFree(formatAllocationData);

    return;
}


extern "C" BSTR WQLQueryBuilder(const wchar_t* fmt) {
    wchar_t buffer[512];

    MSVCRT$_snwprintf(
        buffer, 
        sizeof(buffer) / sizeof(wchar_t), 
        L"ASSOCIATORS OF {Win32_Service.Name=\"%s\"} WHERE AssocClass=Win32_DependentService Role=Antecedent", 
        fmt
    );

    return OLEAUT32$SysAllocString(buffer);
}

extern "C" void go(char* argc, int len)
{
    datap   arguments;
    WCHAR*  wServiceName = NULL;

    BeaconDataParse(&arguments, argc, len);

    wServiceName = (wchar_t*)BeaconDataExtract(&arguments, NULL);

    BSTR bstrQuery = WQLQueryBuilder((const WCHAR*)wServiceName);
    if (bstrQuery == NULL)
    {
        return;
    }

    BSTR bWQL = OLEAUT32$SysAllocString(L"WQL");
    if (bWQL == NULL)
    {
        OLEAUT32$SysFreeString(bstrQuery);

        return;
    }

    BSTR bRoot = OLEAUT32$SysAllocString(L"ROOT\\CIMV2");
    if (bRoot == NULL)
    {
        OLEAUT32$SysFreeString(bstrQuery);
        OLEAUT32$SysFreeString(bWQL);

        return;
    }

    HRESULT hres = OLE32$CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hres))
    {
        BeaconPrintf(CALLBACK_ERROR, "CoInitializeEx has failed: %08lx", hres);

        OLEAUT32$SysFreeString(bstrQuery);
        OLEAUT32$SysFreeString(bWQL);
        OLEAUT32$SysFreeString(bRoot);

        return;
    }

    hres = OLE32$CoInitializeSecurity(
            NULL,
            -1,                          
            NULL,                        
            NULL,                        
            RPC_C_AUTHN_LEVEL_DEFAULT,   
            RPC_C_IMP_LEVEL_IMPERSONATE,  
            NULL,                        
            EOAC_NONE,                    
            NULL
    );

    IWbemLocator* pLoc = NULL;

    hres = OLE32$CoCreateInstance(
                g_CLSID_WbemLocator,
                0,
                CLSCTX_INPROC_SERVER,
                g_IID_IWbemLocator, 
                (LPVOID*)&pLoc
            );

    if (FAILED(hres))
    {
        BeaconPrintf(CALLBACK_ERROR, "CoCreateInstance has failed: %08lx", hres);

        OLEAUT32$SysFreeString(bstrQuery);
        OLEAUT32$SysFreeString(bWQL);
        OLEAUT32$SysFreeString(bRoot);

        OLE32$CoUninitialize();

        return;
    }

    IWbemServices* pSvc = NULL;

    hres = pLoc->ConnectServer(
        bRoot,
        NULL,
        NULL,
        0,
        0,
        0,
        0,
        &pSvc
    );

    if (FAILED(hres))
    {
        if (pLoc != NULL)
        {
            pLoc->Release();
        }

        OLEAUT32$SysFreeString(bstrQuery);
        OLEAUT32$SysFreeString(bWQL);
        OLEAUT32$SysFreeString(bRoot);

        OLE32$CoUninitialize();

        return;
    }

    hres = OLE32$CoSetProxyBlanket(
                pSvc, 
                RPC_C_AUTHN_WINNT, 
                RPC_C_AUTHZ_NONE,
                NULL, 
                RPC_C_AUTHN_LEVEL_CALL, 
                RPC_C_IMP_LEVEL_IMPERSONATE,
                NULL, 
                EOAC_NONE
            );

    if (FAILED(hres))
    {
        if (pSvc != NULL)
        {
            pSvc->Release();
        }

        if (pLoc != NULL)
        {
            pLoc->Release();
        }
        
        OLEAUT32$SysFreeString(bstrQuery);
        OLEAUT32$SysFreeString(bWQL);
        OLEAUT32$SysFreeString(bRoot);

        OLE32$CoUninitialize();

        return;
    }
    
    IEnumWbemClassObject* pEnumerator;
    hres = pSvc->ExecQuery(
        bWQL,
        bstrQuery,
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL, 
        &pEnumerator
    );

    if (FAILED(hres)) {
        if (pSvc != NULL)
        {
            pSvc->Release();
        }

        if (pLoc != NULL)
        {
            pLoc->Release();
        }

        OLEAUT32$SysFreeString(bstrQuery);
        OLEAUT32$SysFreeString(bWQL);
        OLEAUT32$SysFreeString(bRoot);

        OLE32$CoUninitialize();
    }

    IWbemClassObject* pObj;
    ULONG returned;
    INT index = 1;
    formatp fpObject;
    BeaconFormatAlloc(&fpObject, 64 * 1024);

    while (pEnumerator->Next(WBEM_INFINITE, 1, &pObj, &returned) == S_OK) 
    {
        VARIANT vtName;
        OLEAUT32$VariantInit(&vtName);

        if (SUCCEEDED(pObj->Get(L"Name", 0, &vtName, 0, 0)) && vtName.vt == VT_BSTR) 
        {
            BeaconFormatPrintf(&fpObject, "\t[%02d] %S\n", index++, vtName.bstrVal);
        }

        OLEAUT32$VariantClear(&vtName);
    }

    if (pSvc != NULL)
    {
        pSvc->Release();
    }

    if (pLoc != NULL)
    {
        pLoc->Release();
    }
    
    OLEAUT32$SysFreeString(bstrQuery);
    OLEAUT32$SysFreeString(bWQL);
    OLEAUT32$SysFreeString(bRoot);

    OLE32$CoUninitialize();

    dumpFormatAllocation(&fpObject, (const WCHAR*)wServiceName);

    return;
    
}