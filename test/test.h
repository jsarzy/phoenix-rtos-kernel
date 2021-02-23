/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Test subsystem
 *
 * Copyright 2012, 2016, 2017 Phoenix Systems
 * Copyright 2001, 2005 Pawel Pisarczyk
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _TEST_TEST_H_
#define _TEST_TEST_H_

#include "vm.h"
#include "proc.h"

#ifdef KERNEL_TEST
#include "unity_fixture.h"

#define KERNEL_TEST_TASK_PRIO 7

void test_main(void* args);

#endif


#endif
