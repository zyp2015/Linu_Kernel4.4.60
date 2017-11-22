/*
 * This is the 1999 rewrite of IP Firewalling, aiming for kernel 2.3.x.
 *
 * Copyright (C) 1999 Paul `Rusty' Russell & Michael J. Neuling
 * Copyright (C) 2000-2004 Netfilter Core Team <coreteam@netfilter.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/slab.h>
#include <net/ip.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Netfilter Core Team <coreteam@netfilter.org>");
MODULE_DESCRIPTION("iptables filter table");

#define FILTER_VALID_HOOKS ((1 << NF_INET_LOCAL_IN) | \
			    (1 << NF_INET_FORWARD) | \
			    (1 << NF_INET_LOCAL_OUT))
/*filter ��ṹ��*/
static const struct xt_table packet_filter = {
	.name		= "filter",
	.valid_hooks	= FILTER_VALID_HOOKS,/*Ӱ��Ĺ��ӵ�*/
	.me		= THIS_MODULE,
	.af		= NFPROTO_IPV4,
	.priority	= NF_IP_PRI_FILTER,
};

static unsigned int
iptable_filter_hook(void *priv, struct sk_buff *skb,/*filter���hook���� ��󶼻����ipt_do_table��������*/
		    const struct nf_hook_state *state)
{
	if (state->hook == NF_INET_LOCAL_OUT &&
	    (skb->len < sizeof(struct iphdr) ||
	     ip_hdrlen(skb) < sizeof(struct iphdr)))
		/* root is playing with raw sockets. */
		return NF_ACCEPT;
/*state->net->ipv4.iptable_filter �������filer�� ����洢�˹�����صĶ���*/

	return ipt_do_table(skb, state, state->net->ipv4.iptable_filter);

	/*net->ipv4.iptable_filter �������
	iptable_filter_net_init ���︳ֵ�� ��net����ip_rcv���������dev_net������ȥ��  */
}

static struct nf_hook_ops *filter_ops  ;/*filer ��Ĺ���ע��ṹ�� */

/* Default to forward because I got too much mail already. */
static bool forward = true;
module_param(forward, bool, 0000);
/*�������ں�����ռ���صĶ��� filer ����ռ�ĳ�ʼ�� ��֪����üӽ�netfilter��ܵ�*/
static int __net_init iptable_filter_net_init(struct net *net)
{
	struct ipt_replace *repl;/*��Ľṹ��*/

	repl = ipt_alloc_initial_table(&packet_filter);/*����packet_filter ��ʼ��filer�� �������ע�� �洢�����������һ���ṹ�� */
	if (repl == NULL)
		return -ENOMEM;
	/* Entry 1 is the FORWARD hook */
	((struct ipt_standard *)repl->entries)[1].target.verdict =
		forward ? -NF_ACCEPT - 1 : -NF_DROP - 1;

	net->ipv4.iptable_filter =/*����ռ�ı�ֵ*/
		ipt_register_table(net, &packet_filter, repl);/*ע��filer��*/
	kfree(repl);
	return PTR_ERR_OR_ZERO(net->ipv4.iptable_filter);
}

static void __net_exit iptable_filter_net_exit(struct net *net)
{
	ipt_unregister_table(net, net->ipv4.iptable_filter);
}

static struct pernet_operations iptable_filter_net_ops = {
	.init = iptable_filter_net_init,
	.exit = iptable_filter_net_exit,
};

/*filer ģ���ʼ��*/
static int __init iptable_filter_init(void)
{
	int ret;

	ret = register_pernet_subsys(&iptable_filter_net_ops);/*����ռ�Ķ��� ��ȥִ��iptable_filter_net_ops�ṹ������Ӧ�ĺ���*/
	if (ret < 0)
		return ret;

	/* Register hooks */
	/*ע��filer����ص�hook����*/
	filter_ops = xt_hook_link(&packet_filter, iptable_filter_hook);/*�������ע��hook������ ����packet_filter��FILTER_VALID_HOOKS 
	                                                                1�ĸ�����ע�ṳ�Ӻ��� 1��λ��Ҳ������hook��*/
	if (IS_ERR(filter_ops)) {
		ret = PTR_ERR(filter_ops);
		unregister_pernet_subsys(&iptable_filter_net_ops);
	}

	return ret;
}

static void __exit iptable_filter_fini(void)
{
	xt_hook_unlink(&packet_filter, filter_ops);
	unregister_pernet_subsys(&iptable_filter_net_ops);
}

module_init(iptable_filter_init);
module_exit(iptable_filter_fini);
