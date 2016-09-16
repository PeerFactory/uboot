/*
 *  include/ns921x.h
 *
 *  Copyright (C) 2007 by Digi International Inc.
 *  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version2  as published by
 *  the Free Software Foundation.
*/
/*
 *  !Revision:   $Revision$
 *  !Author:     Markus Pietrek
 *  !Descr:      ns921x helper functions
*/

#ifndef _NS921X_H
#define _NS921X_H

#define NS_CPU_REF_CLOCK 29491200

#ifndef __ASSEMBLY__
extern int sys_clock_freq( void );
extern int cpu_clock_freq( void );
extern int ahb_clock_freq( void );
#endif

#endif  /* _NS921X_H */
