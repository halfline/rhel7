/*
 * rh_wrap.c - RHEL specific wrappers
 */

#include <linux/netdevice.h>

int __rh_call_ndo_setup_tc(struct net_device *dev, u32 handle, __be16 protocol,
			   struct tc_to_netdev *tc)
{
	const struct net_device_ops *ops = dev->netdev_ops;

	if (ops->ndo_setup_tc) {
		return ops->ndo_setup_tc(dev, handle, protocol, tc);
	} else if (ops->ndo_setup_tc_rh72 && tc->type == TC_SETUP_MQPRIO) {
		/* Note that drivers that implement .ndo_setup_tc_rh72() can
		 * only support mqprio so this entry-point can be called
		 * only for this type
		 */
		return ops->ndo_setup_tc_rh72(dev, tc->tc);
	}

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(__rh_call_ndo_setup_tc);
