/*
 * vrrp_options.c
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

#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>

#include "vrrp.h"
#include "vrrp_net.h"
#include "vrrp_options.h"
#include "common.h"
#include "log.h"

/* from uvrrpd.c */
extern int background;
extern char *loglevel;
extern char *pidfile_name;

/**
 * vrrp_usage()
 */
static void vrrp_usage(void)
{
	fprintf(stdout,
		"Usage: uvrrpd -v vrid -i ifname [OPTIONS] VIP1 [… VIPn]\n\n"
		"Mandatory options:\n"
		"  -v, --vrid vrid           Virtual router identifier\n"
		"  -i, --interface iface     Interface\n"
		"  VIP                       Virtual IP(s), 1 to 255 VIPs\n\n"
		"Optional arguments:\n"
		"  -p, --priority prio       Priority of VRRP Instance, (0-255, default 100)\n"
		"  -t, --time delay          Time interval between advertisements\n"
		"                            Seconds in VRRPv2 (default 1s),\n"
		"                            Centiseconds in VRRPv3 (default 100cs)\n"
		"  -P, --preempt on|off      Switch preempt (default on)\n"
		"  -r, --rfc version         Specify protocol 'version'\n"
		"                            2 (VRRPv2, RFC3768) by default,\n"
		"                            3 (VRRPv3, RFC5798)\n"
		"  -6, --ipv6                IPv6 support, (only in VRRPv3)\n"
		"  -a, --auth pass           Simple text password (only in VRRPv2)\n"
		"  -f, --foreground          Execute uvrrpd in foreground\n"
		"  -s, --script              Path of hook script (default /etc/uvrrpd/uvrrpd-switch.sh)\n"
		"  -F  --pidfile name        Create pid file 'name'\n"
		"                            Default /var/run/uvrrp_${vrid}.pid\n"
		"  -d, --debug\n" "  -h, --help\n");
}

/**
 * vrrp_options() - Parse command line options
 */
int vrrp_options(struct vrrp *vrrp, struct vrrp_net *vnet, int argc,
		 char *argv[])
{
	int optc, err;
	unsigned long opt;	/* strtoul */

	static struct option const opts[] = {
		{"vrid", required_argument, 0, 'v'},
		{"interface", required_argument, 0, 'i'},
		{"priority", required_argument, 0, 'p'},
		{"time", required_argument, 0, 't'},
		{"preempt", required_argument, 0, 'P'},
		{"rfc", required_argument, 0, 'r'},
		{"ipv6", no_argument, 0, '6'},
		{"auth", required_argument, 0, 'a'},
		{"foreground", no_argument, 0, 'f'},
		{"script", required_argument, 0, 's'},
		{"pidfile", required_argument, 0, 'F'},
		{"debug", no_argument, 0, 'd'},
		{"help", no_argument, 0, 'h'},
		{NULL, 0, 0, 0}
	};

	while ((optc =
		getopt_long(argc, argv, "v:i:p:t:P:r:6a:fs:F:dh", opts,
			    NULL)) != EOF) {
		switch (optc) {

			/* vrid */
		case 'v':
			err = mystrtoul(&opt, optarg, VRID_MAX);
			if (err == -ERANGE || (err == 0 && opt == 0)) {
				fprintf(stderr, "1 < vrid < 255\n");
				vrrp_usage();
				return err;
			}
			if (err == -EINVAL) {
				fprintf(stderr,
					"Error parsing \"%s\" as a number\n",
					optarg);
				vrrp_usage();
				return err;
			}

			vrrp->vrid = vnet->vrid = (uint8_t)opt;
			break;

			/* interface */
		case 'i':
			vnet->vif.ifname = strndup(optarg, IFNAMSIZ);
			if (vnet->vif.ifname == NULL) {
				perror("strndup");
				return -1;
			}
			break;

			/* priority */
		case 'p':
			err = mystrtoul(&opt, optarg, VRRP_PRIO_MAX);
			if (err == -ERANGE) {
				fprintf(stderr, "0 < priority < 255\n");
				vrrp_usage();
				return -1;
			}
			if (err == -EINVAL) {
				fprintf(stderr,
					"Error parsing \"%s\" as a number\n",
					optarg);
				vrrp_usage();
				return err;
			}

			vrrp->priority = (uint8_t)opt;
			break;

			/* delay */
		case 't':
			err = mystrtoul(&opt, optarg, ADVINT_MAX);
			if (err == -ERANGE) {
				vrrp_usage();
				return -1;
			}
			if (err == -EINVAL) {
				fprintf(stderr,
					"Error parsing \"%s\" as a number\n",
					optarg);
				vrrp_usage();
				return err;
			}

			vrrp->adv_int = (uint16_t)opt;
			break;

			/* preempt mode */
		case 'P':
			if (matches(optarg, "on"))
				vrrp->preempt = TRUE;
			else if (matches(optarg, "off"))
				vrrp->preempt = FALSE;
			else {
				fprintf(stderr,
					"preempt mode 'on' or 'off', by default 'on'\n");
				vrrp_usage();
				return -1;
			}
			break;

			/* RFC - version */
		case 'r':
			err = mystrtoul(&opt, optarg, RFC5798);
			if (err == -ERANGE || (err == 0 && opt < RFC3768)) {
				fprintf(stderr, "Version 2 or 3 : %s\n", optarg);
				vrrp_usage();
				return -1;
			}
			if (err == -EINVAL) {
				fprintf(stderr,
					"Error parsing \"%s\" as a number\n",
					optarg);
				vrrp_usage();
				return err;
			}

			vrrp->version = (uint8_t) opt;
			break;

			/* IPv6 */
		case '6':	/* Force RFC5798/VRRPv3 */
			vrrp->version = RFC5798;
			vnet->family = AF_INET6;
			break;

			/* auth */
		case 'a':	/* only SIMPLE password supported */
			if (strlen(optarg) > VRRP_AUTH_PASS_LEN) {
				fprintf(stderr,
					"Password too long (8 char max)\n");
				vrrp_usage();
				return -1;
			}
			vrrp->auth_data = strndup(optarg, VRRP_AUTH_PASS_LEN);
			vrrp->auth_type = SIMPLE;
			/* hide passwd from ps */
			strncpy(optarg, "********", strlen(optarg));
			break;

			/* foreground */
		case 'f':
			background = 0;
			break;


			/* script */
		case 's':
			vrrp->scriptname = strndup(optarg, VRRP_SCRIPT_MAX);
			if (vrrp->scriptname == NULL) {
				perror("strndup");
				return -1;
			}
			break;

			/* pidfile */
		case 'F':
			pidfile_name = strndup(optarg, NAME_MAX + PATH_MAX);
			break;

			/* debug */
		case 'd':
			loglevel = strndup("debug", 6);
			break;

			/* help */
		case 'h':
		default:
			vrrp_usage();
			return -1;
			break;
		}
	}

	/* Fetch virtual IP addresses */
	if (optind == argc) {
		fprintf(stderr, "Specify at least one virtual IP addr !\n");
		vrrp_usage();
		return -1;
	}

	/* Number of IP addresses */
	vrrp->naddr = vnet->naddr = argc - optind;

	/* Register vrrp_vip addresses */
	while (optind != argc) {
		if (vrrp_net_vip_set(vnet, argv[optind]) != 0) {
			fprintf(stderr, "Invalid IP\n");
			vrrp_usage();
			return -1;
		}
		++optind;
	}

	/* minimal configuration */
	if (vrrp->vrid == 0) {
		fprintf(stderr, "Specify VRRP id\n");
		vrrp_usage();
		return -1;
	}
	if (vnet->vif.ifname == NULL) {
		fprintf(stderr, "Specify VRRP interface\n");
		vrrp_usage();
		return -1;
	}

	/* default adv int */
	if ((vrrp->version == RFC3768) && (vrrp->adv_int == 0))
		vrrp->adv_int = 1;
	else if ((vrrp->version == RFC5798) && (vrrp->adv_int == 0))
		vrrp->adv_int = 100;

	/* Get IP addresse from interface name */
	return vrrp_net_vif_getaddr(vnet);
}
