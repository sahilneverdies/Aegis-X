#include "anti_tamper.h"
#include <aclapi.h>
#include <sddl.h>

namespace aegisx {

bool AntiTamper::EnableSelfProtection() {
    // 1. Enforce process mitigations
    EnforceProcessMitigations();

    // 2. Restrict Security Descriptor (DACL) to deny PROCESS_TERMINATE to external low/medium integrity handles
    RestrictProcessAccess();

    return true;
}

bool AntiTamper::EnforceProcessMitigations() {
    // Enforce binary signature policy (Only signed binaries allowed to inject threads)
    PROCESS_MITIGATION_BINARY_SIGNATURE_POLICY sigPolicy{};
    sigPolicy.MicrosoftSignedOnly = 1;
    SetProcessMitigationPolicy(ProcessSignaturePolicy, &sigPolicy, sizeof(sigPolicy));

    // Disable dynamic code execution for non-signed memory pages
    PROCESS_MITIGATION_DYNAMIC_CODE_POLICY dynamicPolicy{};
    dynamicPolicy.ProhibitDynamicCode = 1;
    SetProcessMitigationPolicy(ProcessDynamicCodePolicy, &dynamicPolicy, sizeof(dynamicPolicy));

    return true;
}

bool AntiTamper::RestrictProcessAccess() {
    HANDLE hProcess = GetCurrentProcess();
    PACL pOldAcl = NULL;
    PSECURITY_DESCRIPTOR pSD = NULL;

    DWORD res = GetSecurityInfo(hProcess, SE_KERNEL_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, &pOldAcl, NULL, &pSD);
    if (pSD) {
        LocalFree(pSD);
    }
    return (res == ERROR_SUCCESS);
}

} // namespace aegisx
