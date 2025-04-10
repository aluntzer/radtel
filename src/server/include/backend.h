/**
 * @file    server/include/backend.h
 * @author  Armin Luntzer (armin.luntzer@univie.ac.at)
 *
 * @copyright GPLv2
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef _SERVER_INCLUDE_BACKEND_H_
#define _SERVER_INCLUDE_BACKEND_H_

#include <gmodule.h>
#include <protocol.h>

/* backend calls */
int be_moveto_azel(double az, double el);
void be_shared_comlink_acquire(void);
void be_shared_comlink_release(void);
int be_shared_comlink_write(const gchar *buf, gsize nbytes);
gchar *be_shared_comlink_read(gsize *nbytes);
void be_recalibrate_pointing(void);
void be_park_telescope(void);
int be_spec_acq_cfg(struct spec_acq_cfg *acq);
int be_getpos_azel(double *az, double *el);
int be_spec_acq_enable(gboolean mode);
int be_spec_acq_cfg_get(struct spec_acq_cfg *acq);
int be_get_capabilities_drive(struct capabilities *c);
int be_get_capabilities_spec(struct capabilities *c);
int be_get_capabilities_load_drive(struct capabilities_load *c);
int be_get_capabilities_load_spec(struct capabilities_load *c);
int be_hot_load_enable(gboolean mode);
int be_radiometer_pwr_ctrl(gboolean mode);
int be_drive_pwr_ctrl(gboolean mode);
void be_drive_pwr_cycle(void);
gboolean be_drive_pwr_status(void);

/* backend call loaders */
int be_moveto_azel_load(GModule *mod);
int be_shared_comlink_acquire_load(GModule *mod);
int be_shared_comlink_release_load(GModule *mod);
int be_shared_comlink_write_load(GModule *mod);
int be_shared_comlink_read_load(GModule *mod);
int be_recalibrate_pointing_load(GModule *mod);
int be_park_telescope_load(GModule *mod);
int be_spec_acq_cfg_load(GModule *mod);
int be_spec_acq_cfg_get_load(GModule *mod);
int be_getpos_azel_load(GModule *mod);
int be_spec_acq_enable_load(GModule *mod);
int be_get_capabilities_drive_load(GModule *mod);
int be_get_capabilities_spec_load(GModule *mod);
int be_get_capabilities_load_drive_load(GModule *mod);
int be_get_capabilities_load_spec_load(GModule *mod);
int be_hot_load_enable_load(GModule *mde);
int be_radiometer_pwr_ctrl_load(GModule *mde);
int be_drive_pwr_ctrl_load(GModule *mde);
int be_drive_pwr_cycle_load(GModule *mde);
int be_drive_pwr_status_load(GModule *mde);


int backend_load_plugins(void);



#endif /* _SERVER_INCLUDE_BACKEND_H_ */

