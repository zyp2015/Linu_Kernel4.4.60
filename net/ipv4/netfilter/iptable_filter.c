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
/*filter 表结构体*/
static const struct xt_table packet_filter = {
	.name		= "filter",
	.valid_hooks	= FILTER_VALID_HOOKS,/*影响的钩子点*/
	.me		= THIS_MODULE,
	.af		= NFPROTO_IPV4,
	.priority	= NF_IP_PRI_FILTER,
};

static unsigned int
iptable_filter_hook(void *priv, struct sk_buff *skb,/*filter表的hook函数 最后都会调用ipt_do_table遍历规则*/
		    const struct nf_hook_state *state)
{
	if (state->hook == NF_INET_LOCAL_OUT &&
	    (skb->len < sizeof(struct iphdr) ||
	     ip_hdrlen(skb) < sizeof(struct iphdr)))
		/* root is playing with raw sockets. */
		return NF_ACCEPT;
/*state->net->ipv4.iptable_filter 这个就是filer表 里面存储了规则相关的东西*/

	return ipt_do_table(skb, state, state->net->ipv4.iptable_filter);

	/*net->ipv4.iptable_filter 这个是在
	iptable_filter_net_init 这里赋值的 而net则是ip_rcv函数里调用dev_net函数过去的  */
}

static struct nf_hook_ops *filter_ops  ;/*filer 表的钩子注册结构体 */

/* Default to forward because I got too much mail already. */
static bool forward = true;
module_param(forward, bool, 0000);
/*好像是内核网络空间相关的东西 filer 网络空间的初始化 不知道多久加进netfilter框架的*/
static int __net_init iptable_filter_net_init(struct net *net)
{
	struct ipt_replace *repl;/*表的结构体*/

	repl = ipt_alloc_initial_table(&packet_filter);/*根据packet_filter 初始化filer表 方便后面注册 存储规则的是另外一个结构体 */
	if (repl == NULL)
		return -ENOMEM;
	/* Entry 1 is the FORWARD hook */
	((struct ipt_standard *)repl->entries)[1].target.verdict =
		forward ? -NF_ACCEPT - 1 : -NF_DROP - 1;

	net->ipv4.iptable_filter =/*网络空间的表赋值*/
		ipt_register_table(net, &packet_filter, repl);/*注册filer表*/
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

/*filer 模块初始化*/
static int __init iptable_filter_init(void)
{
	int ret;

	ret = register_pernet_subsys(&iptable_filter_net_ops);/*网络空间的东西 会去执行iptable_filter_net_ops结构体里相应的函数*/
	if (ret < 0)
		return ret;

	/* Register hooks */
	/*注册filer表相关的hook函数*/
	filter_ops = xt_hook_link(&packet_filter, iptable_filter_hook);/*这里就是注册hook函数了 根据packet_filter中FILTER_VALID_HOOKS 
	                                                                1的个数来注册钩子函数 1的位置也代表着hook点*/
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
