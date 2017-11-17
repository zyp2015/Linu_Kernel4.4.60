/*
 *	Handle incoming frames
 *	Linux ethernet bridge
 *
 *	Authors:
 *	Lennert Buytenhek		<buytenh@gnu.org>
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */

#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/netfilter_bridge.h>
#include <linux/neighbour.h>
#include <net/arp.h>
#include <linux/export.h>
#include <linux/rculist.h>
#include "br_private.h"

/* Hook for brouter */
br_should_route_hook_t __rcu *br_should_route_hook  ;
EXPORT_SYMBOL(br_should_route_hook);

static int
br_netif_receive_skb(struct net *net, struct sock *sk, struct sk_buff *skb)
{
	br_drop_fake_rtable(skb);
	/*调用 netif_receive_skb 重新进入协议栈进行处理 因为br的rx_handler没有被设置 从而进入上层协议栈*/
	return netif_receive_skb(skb);
}

/* Hook for external Multicast handler */
br_multicast_handle_hook_t __rcu *br_multicast_handle_hook  ;
EXPORT_SYMBOL_GPL(br_multicast_handle_hook);

/* Hook for external forwarding logic */
br_get_dst_hook_t __rcu *br_get_dst_hook  ;
EXPORT_SYMBOL_GPL(br_get_dst_hook);

int br_pass_frame_up(struct sk_buff *skb)
{
	struct net_device *indev, *brdev = BR_INPUT_SKB_CB(skb)->brdev;
	struct net_bridge *br = netdev_priv(brdev);
	struct net_bridge_vlan_group *vg;
	struct pcpu_sw_netstats *brstats = this_cpu_ptr(br->stats);

	u64_stats_update_begin(&brstats->syncp);
	brstats->rx_packets++;
	brstats->rx_bytes += skb->len;
	u64_stats_update_end(&brstats->syncp);

	vg = br_vlan_group_rcu(br);
	/* Bridge is just like any other port.  Make sure the
	 * packet is allowed except in promisc modue when someone
	 * may be running packet capture.
	 */
	if (!(brdev->flags & IFF_PROMISC) &&
	    !br_allowed_egress(vg, skb)) {
		kfree_skb(skb);
		return NET_RX_DROP;
	}
    /*这里更改了skb的dev为br，而不是br下的设备*/
	indev = skb->dev;
	skb->dev = brdev;
	skb = br_handle_vlan(br, vg, skb);
	if (!skb)
		return NET_RX_DROP;
    /*进入netfilter处理之后又调用 br_netif_receive_skb*/
	return BR_HOOK(NFPROTO_BRIDGE, NF_BR_LOCAL_IN,
		       dev_net(indev), NULL, skb, indev, NULL,
		       br_netif_receive_skb);
}
EXPORT_SYMBOL_GPL(br_pass_frame_up);

static void br_do_proxy_arp(struct sk_buff *skb, struct net_bridge *br,
			    u16 vid, struct net_bridge_port *p)
{
	struct net_device *dev = br->dev;
	struct neighbour *n;
	struct arphdr *parp;
	u8 *arpptr, *sha;
	__be32 sip, tip;

	BR_INPUT_SKB_CB(skb)->proxyarp_replied = false;
    printk(KERN_ERR "SKYSOFT-DBG:HHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHH\n");
    if(!skb->nf_bridge){
       skb->nf_bridge = kzalloc(sizeof(struct nf_bridge_info), GFP_ATOMIC);
       if (likely(skb->nf_bridge)) {
             atomic_set(&(skb->nf_bridge->use), 1);
             skb->nf_bridge->physindev = skb->dev;
             printk(KERN_ERR "SKYSOFT-DBG:%s\n",skb->nf_bridge->physindev);
       }
    }
	if (dev->flags & IFF_NOARP)
		return;

	if (!pskb_may_pull(skb, arp_hdr_len(dev))) {
		dev->stats.tx_dropped++;
		return;
	}
	parp = arp_hdr(skb);

	if (parp->ar_pro != htons(ETH_P_IP) ||
	    parp->ar_op != htons(ARPOP_REQUEST) ||
	    parp->ar_hln != dev->addr_len ||
	    parp->ar_pln != 4)
		return;

	arpptr = (u8 *)parp + sizeof(struct arphdr);
	sha = arpptr;
	arpptr += dev->addr_len;	/* sha */
	memcpy(&sip, arpptr, sizeof(sip));
	arpptr += sizeof(sip);
	arpptr += dev->addr_len;	/* tha */
	memcpy(&tip, arpptr, sizeof(tip));

	if (ipv4_is_loopback(tip) ||
	    ipv4_is_multicast(tip))
		return;

	n = neigh_lookup(&arp_tbl, &tip, dev);
	if (n) {
		struct net_bridge_fdb_entry *f;

		if (!(n->nud_state & NUD_VALID)) {
			neigh_release(n);
			return;
		}

		f = __br_fdb_get(br, n->ha, vid);
		if (f && ((p->flags & BR_PROXYARP) ||
			  (f->dst && (f->dst->flags & BR_PROXYARP_WIFI)))) {
			arp_send(ARPOP_REPLY, ETH_P_ARP, sip, skb->dev, tip,
				 sha, n->ha, sha);
			BR_INPUT_SKB_CB(skb)->proxyarp_replied = true;
		}

		neigh_release(n);
	}
}

/* note: already called with rcu_read_lock */
int br_handle_frame_finish(struct net *net, struct sock *sk, struct sk_buff *skb)
{
	const unsigned char *dest = eth_hdr(skb)->h_dest;
	struct net_bridge_port *p = br_port_get_rcu(skb->dev);
	struct net_bridge *br;
	struct net_bridge_fdb_entry *dst;
	struct net_bridge_mdb_entry *mdst;
	struct sk_buff *skb2;
	struct net_bridge_port *pdst = NULL;
	br_get_dst_hook_t *get_dst_hook = rcu_dereference(br_get_dst_hook);
	bool unicast = true;
	u16 vid = 0;

	if (!p || p->state == BR_STATE_DISABLED)
		goto drop;
		
    /*这个判断主要是vlan的相关检查，如是否和接收接口配置的vlan相同*/
	if (!br_allowed_ingress(p->br, nbp_vlan_group_rcu(p), skb, &vid))
		goto out;

	/* insert into forwarding database after filtering to avoid spoofing */
	br = p->br; 
	if (p->flags & BR_LEARNING)
	    /*更新转发数据库*/
		br_fdb_update(br, p, eth_hdr(skb)->h_source, vid, false);
        /*多播或者广播处理*/
	if (!is_broadcast_ether_addr(dest) && is_multicast_ether_addr(dest) &&
	    br_multicast_rcv(br, p, skb, vid))
		goto drop;

	if ((p->state == BR_STATE_LEARNING) && skb->protocol != htons(ETH_P_PAE))
		goto drop;

	BR_INPUT_SKB_CB(skb)->brdev = br->dev;

	/* The packet skb2 goes to the local host (NULL to skip). */
	skb2 = NULL;

	if (br->dev->flags & IFF_PROMISC)
		skb2 = skb;

	dst = NULL;
    
	if (IS_ENABLED(CONFIG_INET) && skb->protocol == htons(ETH_P_ARP))
		br_do_proxy_arp(skb, br, vid, p);

	if (skb->protocol == htons(ETH_P_PAE)) {
		skb2 = skb;
		/* Do not forward 802.1x/EAP frames */
		skb = NULL;
	} else if (is_broadcast_ether_addr(dest)) {
		skb2 = skb;
		unicast = false;
	} else if (is_multicast_ether_addr(dest)) {
		br_multicast_handle_hook_t *multicast_handle_hook =
			rcu_dereference(br_multicast_handle_hook);
		if (!__br_get(multicast_handle_hook, true, p, skb))
			goto out;

		mdst = br_mdb_get(br, skb, vid);
		if ((mdst || BR_INPUT_SKB_CB_MROUTERS_ONLY(skb)) &&
		    br_multicast_querier_exists(br, eth_hdr(skb))) {
			if ((mdst && mdst->mglist) ||
			    br_multicast_is_router(br))
				skb2 = skb;
			br_multicast_forward(mdst, skb, skb2);
			skb = NULL;
			if (!skb2)
				goto out;
		} else
			skb2 = skb;

		unicast = false;
		br->dev->stats.multicast++;
	} else {
		pdst = __br_get(get_dst_hook, NULL, p, &skb);
		if (pdst) {
			if (!skb)
				goto out;
		} else {
			dst = __br_fdb_get(br, dest, vid);
			if ((p->flags & BR_ISOLATE_MODE) ||
			    (dst && dst->is_local)) {
				skb2 = skb;
				/* Do not forward the packet since it's local.*/
				skb = NULL;
			}
		}
	}

	if (skb) {
		if (dst) {
			dst->used = jiffies;
			pdst = dst->dst;
		}

		if (pdst)
			br_forward(pdst, skb, skb2);
		else
			br_flood_forward(br, skb, skb2, unicast);
	}

	if (skb2)//发往本机协议栈
		return br_pass_frame_up(skb2);

out:
	return 0;
drop:
	kfree_skb(skb);
	goto out;
}
EXPORT_SYMBOL_GPL(br_handle_frame_finish);

/* note: already called with rcu_read_lock */
static int br_handle_local_finish(struct net *net, struct sock *sk, struct sk_buff *skb)
{
	struct net_bridge_port *p = br_port_get_rcu(skb->dev);
	if (p->state != BR_STATE_DISABLED) {
		u16 vid = 0;

		/* check if vlan is allowed, to avoid spoofing */
		if (p->flags & BR_LEARNING && br_should_learn(p, skb, &vid))
			br_fdb_update(p->br, p, eth_hdr(skb)->h_source, vid, false);
	}
	return 0;	 /* process further */
}

/*
 * Return NULL if skb is handled
 * note: already called with rcu_read_lock
 */
rx_handler_result_t br_handle_frame(struct sk_buff **pskb)
{
	struct net_bridge_port *p;
	struct sk_buff *skb = *pskb;
	const unsigned char *dest = eth_hdr(skb)->h_dest;
	br_should_route_hook_t *rhook;

	if (unlikely(skb->pkt_type == PACKET_LOOPBACK))
		return RX_HANDLER_PASS;

	if (!is_valid_ether_addr(eth_hdr(skb)->h_source))
		goto drop;

	skb = skb_share_check(skb, GFP_ATOMIC);
	if (!skb)
		return RX_HANDLER_CONSUMED;
    /*获取dev对应的bridge port*/
	p = br_port_get_rcu(skb->dev);
    /*特殊MAC地址处理*/
	if (unlikely(is_link_local_ether_addr(dest))) {
		u16 fwd_mask = p->br->group_fwd_mask_required;

		/*
		 * See IEEE 802.1D Table 7-10 Reserved addresses
		 *
		 * Assignment		 		Value
		 * Bridge Group Address		01-80-C2-00-00-00
		 * (MAC Control) 802.3		01-80-C2-00-00-01
		 * (Link Aggregation) 802.3	01-80-C2-00-00-02
		 * 802.1X PAE address		01-80-C2-00-00-03
		 *
		 * 802.1AB LLDP 		01-80-C2-00-00-0E
		 *
		 * Others reserved for future standardization
		 */
		switch (dest[5]) {
		case 0x00:	/* Bridge Group Address */
			/* If STP is turned off,
			   then must forward to keep loop detection */
			if (p->br->stp_enabled == BR_NO_STP ||
			    fwd_mask & (1u << dest[5]))
				goto forward;
			break;

		case 0x01:	/* IEEE MAC (Pause) */
			goto drop;

		default:
			/* Allow selective forwarding for most other protocols */
			fwd_mask |= p->br->group_fwd_mask;
			if (fwd_mask & (1u << dest[5]))
				goto forward;
		}

		/* Deliver packet to local host only */
		/* LOCAL_IN hook点，注意经过这个hook点并不代表发送到主机协议栈（只有特殊目的mac 01-80-C2才会走到这里）*/
		if (BR_HOOK(NFPROTO_BRIDGE, NF_BR_LOCAL_IN,
			    dev_net(skb->dev), NULL, skb, skb->dev, NULL,
			    br_handle_local_finish)) {
			return RX_HANDLER_CONSUMED; /* consumed by filter */
		} else {
			*pskb = skb;
			return RX_HANDLER_PASS;	/* continue processing */
		}
	}

forward:
	switch (p->state) {
	case BR_STATE_DISABLED:
		if (skb->protocol == htons(ETH_P_PAE)) {
			if (ether_addr_equal(p->br->dev->dev_addr, dest))
				skb->pkt_type = PACKET_HOST;

			if (BR_HOOK(NFPROTO_BRIDGE, NF_BR_PRE_ROUTING, dev_net(skb->dev), NULL,
				skb, skb->dev, NULL, br_handle_local_finish))
				break;

			BR_INPUT_SKB_CB(skb)->brdev = p->br->dev;
			br_pass_frame_up(skb);
			break;
		}
		goto drop;

	case BR_STATE_FORWARDING:
		rhook = rcu_dereference(br_should_route_hook);
		if (rhook) {
			if ((*rhook)(skb)) {
				*pskb = skb;
				return RX_HANDLER_PASS;
			}
			dest = eth_hdr(skb)->h_dest;
		}
		/* fall through */
	case BR_STATE_LEARNING:
	/*skb的目的mac和bridge的mac一样，则将skb发往本机协议栈*/
		if (ether_addr_equal(p->br->dev->dev_addr, dest))
			skb->pkt_type = PACKET_HOST;

		BR_HOOK(NFPROTO_BRIDGE, NF_BR_PRE_ROUTING,
			dev_net(skb->dev), NULL, skb, skb->dev, NULL,
			br_handle_frame_finish);
		break;
	default:
drop:
		kfree_skb(skb);
	}
	return RX_HANDLER_CONSUMED;
}
