/***************************************************************************
 * eject.c: Eject the CD-ROM drive
 * Copyright (C) 1994-2005 Jeff Tranter (tranter@pobox.com)
 * Copyright (C) 2011-2015 Michael C McTernan, mike@mcternan.uk
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 ***************************************************************************/

/**************************************************************************
 * Includes
 **************************************************************************/

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/cdrom.h>
#include <linux/fs.h>
#include <scsi/scsi_ioctl.h>
#include <scsi/scsi.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "eject.h"
#include "x_mem.h"
#include "log.h"

/**************************************************************************
 * Manifest Constants
 **************************************************************************/

/**************************************************************************
 * Macros
 **************************************************************************/

/**************************************************************************
 * Types
 **************************************************************************/

/**************************************************************************
 * Local Variables
 **************************************************************************/

/**************************************************************************
 * Local Functions
 **************************************************************************/

/**************************************************************************
 * Global Functions
 **************************************************************************/

bool Eject(const char *device)
{
    char cmd[strlen(device) + 9];

    snprintf(cmd, sizeof(cmd), "eject \"%s\"", device);

    if(system(cmd) != EXIT_SUCCESS)
    {
        int fd;

        fd = open(device, O_RDONLY | O_NONBLOCK);
        if(fd == -1)
        {
            LogErr("Error: Failed to open %s: %m\n", device);
            return false;
        }

        if(ioctl(fd, CDROMEJECT) != 0)
        {
            int status;
            struct sdata
            {
                int  inlen;
                int  outlen;
                char cmd[256];
            } scsi_cmd;

            scsi_cmd.inlen  = 0;
            scsi_cmd.outlen = 0;
            scsi_cmd.cmd[0] = ALLOW_MEDIUM_REMOVAL;
            scsi_cmd.cmd[1] = 0;
            scsi_cmd.cmd[2] = 0;
            scsi_cmd.cmd[3] = 0;
            scsi_cmd.cmd[4] = 0;
            scsi_cmd.cmd[5] = 0;
            status = ioctl(fd, SCSI_IOCTL_SEND_COMMAND, (void *)&scsi_cmd);
            if(status != 0)
            {
                return false;
            }

            scsi_cmd.inlen  = 0;
            scsi_cmd.outlen = 0;
            scsi_cmd.cmd[0] = START_STOP;
            scsi_cmd.cmd[1] = 0;
            scsi_cmd.cmd[2] = 0;
            scsi_cmd.cmd[3] = 0;
            scsi_cmd.cmd[4] = 1;
            scsi_cmd.cmd[5] = 0;
            status = ioctl(fd, SCSI_IOCTL_SEND_COMMAND, (void *)&scsi_cmd);
            if(status != 0)
            {
                return false;
            }

            scsi_cmd.inlen  = 0;
            scsi_cmd.outlen = 0;
            scsi_cmd.cmd[0] = START_STOP;
            scsi_cmd.cmd[1] = 0;
            scsi_cmd.cmd[2] = 0;
            scsi_cmd.cmd[3] = 0;
            scsi_cmd.cmd[4] = 2;
            scsi_cmd.cmd[5] = 0;
            status = ioctl(fd, SCSI_IOCTL_SEND_COMMAND, (void *)&scsi_cmd);
            if(status != 0)
            {
                return false;
            }

            /* force kernel to reread partition table when new disc inserted */
            status = ioctl(fd, BLKRRPART);
            return (status == 0);
        }
    }

    return true;
}

/* END OF FILE */

