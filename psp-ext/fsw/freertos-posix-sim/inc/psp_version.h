/*
 * psp_version.h — m2s-cfs freertos-posix-sim PSP version.
 * Follows the stock PSP this platform is built with (equuleus-rc1+dev73, docs/VERSIONS.md, D1).
 */
#ifndef PSP_VERSION_H
#define PSP_VERSION_H

#define CFE_PSP_IMPL_BUILD_NUMBER   73
#define CFE_PSP_IMPL_BUILD_BASELINE "equuleus-rc1"
#define CFE_PSP_BUILD_DEV_CYCLE     "equuleus-rc2"
#define CFE_PSP_BUILD_CODENAME      "Equuleus"

#define CFE_PSP_IMPL_MAJOR_VERSION 1
#define CFE_PSP_IMPL_MINOR_VERSION 4
#define CFE_PSP_IMPL_REVISION      0
#define CFE_PSP_LAST_OFFICIAL      "v1.4.0"
#define CFE_PSP_IMPL_MISSION_REV   0xFF

#define CFE_PSP_IMPL_STR_HELPER(x) #x
#define CFE_PSP_IMPL_STR(x)        CFE_PSP_IMPL_STR_HELPER(x)

#define CFE_PSP_IMPL_VERSION CFE_PSP_IMPL_BUILD_BASELINE "+dev" CFE_PSP_IMPL_STR(CFE_PSP_IMPL_BUILD_NUMBER) "-m2s-freertos"
#define CFE_PSP_CFG_MAX_VERSION_STR_LEN 256

#endif /* PSP_VERSION_H */
