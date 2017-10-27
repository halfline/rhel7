/*
 * rh_wrap.c - RHEL specific wrappers
 */

#include <linux/netdevice.h>

/* Structure tc_to_netdev used by out-of-tree drivers compiled against
 * RHEL7.4 code base.
 */
struct tc_to_netdev_rh74 {
	unsigned int type;
	union {
		u8 tc;
		struct tc_cls_u32_offload *cls_u32;
		struct tc_cls_flower_offload *cls_flower;
		struct tc_cls_matchall_offload *cls_mall;
	};
	bool egress_dev;
};

int __rh_call_ndo_setup_tc(struct net_device *dev, u32 handle, __be16 protocol,
			   struct tc_to_netdev *tc)
{
	const struct net_device_ops *ops = dev->netdev_ops;

	if (get_ndo_ext(ops, ndo_setup_tc)) {
		return get_ndo_ext(ops, ndo_setup_tc)(dev, handle, protocol,
						      tc);
	} else if (ops->ndo_setup_tc_rh74) {
		/* Drivers implementing .ndo_setup_tc_rh74() */
		struct tc_to_netdev_rh74 tc74;

		/* Copy type & egress_dev fields */
		tc74.type = tc->type;
		tc74.egress_dev = tc->egress_dev;

		/* Copy one of the pointer from the union to copy its content */
		tc74.cls_u32 = tc->cls_u32;

		/* The drivers use value tc->tc instead of tc->mqprio->num_tc */
		if (tc->type == TC_SETUP_MQPRIO)
			tc74.tc = tc->mqprio->num_tc;

		return ops->ndo_setup_tc_rh74(dev, handle, protocol, &tc74);
	} else if (ops->ndo_setup_tc_rh72 && tc->type == TC_SETUP_MQPRIO) {
		/* Drivers implementing .ndo_setup_tc_rh72()
		 * Note that drivers that implement .ndo_setup_tc_rh72() can
		 * only support mqprio so this entry-point can be called
		 * only for this type.
		 */
		return ops->ndo_setup_tc_rh72(dev, tc->mqprio->num_tc);
	}

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(__rh_call_ndo_setup_tc);
