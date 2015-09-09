/*                                                                             
 * vrrp_exec.h - export prototype of vrrp_exec()                               
 *                                                                             
 * Copyright (C) 2014 Arnaud Andre                                             
 *                                                                             
 * This file is part of uvrrpd.                                               
 *                                                                             
 * uvrrpd is free software: you can redistribute it and/or modify             
 * it under the terms of the GNU General Public License as published by        
 * the Free Software Foundation, either version 3 of the License, or           
 * (at your option) any later version.                                         
 *                                                                             
 * uvrrpd is distributed in the hope that it will be useful,                  
 * but WITHOUT ANY WARRANTY; without even the implied warranty of              
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the               
 * GNU General Public License for more details.                                
 *                                                                             
 * You should have received a copy of the GNU General Public License           
 * along with uvrrpd.  If not, see <http://www.gnu.org/licenses/>.            
 */

#ifndef _VRRP_EXEC_H_
#define _VRRP_EXEC_H_

#include "vrrp.h"

int vrrp_exec(struct vrrp *vrrp, const struct vrrp_net *vnet, vrrp_state state);
int vrrp_exec_init(struct vrrp *vrrp);
void vrrp_exec_cleanup(struct vrrp *vrrp);

#endif /* _VRRP_EXEC_H_ */
