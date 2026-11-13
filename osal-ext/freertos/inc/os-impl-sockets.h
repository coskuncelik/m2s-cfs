/*
 * os-impl-sockets.h — m2s-cfs OSAL FreeRTOS layer: definitions for OSAL's portable BSD socket code on the Linux
 * simulator (host sockets; step 12a, D41). Same settings as OSAL's POSIX implementation.
 */
#ifndef OS_IMPL_SOCKETS_H
#define OS_IMPL_SOCKETS_H

#include "os-impl-io.h"

#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#define OS_NETWORK_SUPPORTS_IPV6

/* Sockets are non-blocking; every wait goes through OS_SelectSingle_Impl (polling, os-impl-host-sockets.c). */
#define OS_IMPL_SOCKET_SELECTABLE true
#define OS_IMPL_SOCKET_FLAGS      O_NONBLOCK

#endif /* OS_IMPL_SOCKETS_H */
