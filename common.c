#include "common.h"

void __list_add(struct list_head *new,
			      struct list_head *prev,
			      struct list_head *next)
{
	next->prev = new;
	new->next = next;
	new->prev = prev;
	prev->next = new;
}

void list_add_tail(struct list_head *new, struct list_head *head)
{
	__list_add(new, head->prev, head);
}

void list_add(struct list_head *new, struct list_head *head)
{
	__list_add(new, head, head->next);
}

void __list_del(struct list_head * prev, struct list_head * next)
{
	next->prev = prev;
	prev->next = next;
}

#define LIST_POISON1  ((void *) 0x00100100)
#define LIST_POISON2  ((void *) 0x00200200)

void list_del(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
	entry->next = LIST_POISON1;
	entry->prev = LIST_POISON2;
}

void INIT_LIST_HEAD(struct list_head *list)
{
	list->next = list;
	list->prev = list;
}

int call_stack(struct config *conf, int dir)
{
	int ret = 0;
	struct process_handler *handler;
	struct frame frame = {0};
	int more = 1;
	struct list_head *begin;
	struct node_info *tmp_peer;

	dir = !!dir;
	if (dir) {
		begin = conf->first;
	} else {
		begin = conf->last;
	}
	handler = list_entry(begin, struct process_handler, list);
	tmp_peer = NULL;

	while(handler) {
		int preid = handler->id;
		handler->peer = tmp_peer;
		if (dir && handler->send) {
			ret = handler->send(handler, &frame);
		} else if (!dir && handler->receive) {
			ret = handler->receive(handler, &frame);
		}
		if (ret) {
			break;
		}
		tmp_peer = handler->peer;

		if (dir && handler->list.next == &conf->stack) {
			break;
		}
		if (!dir && handler->list.prev == &conf->stack) {
			break;
		}
		if (dir) {
			handler = list_entry(handler->list.next, struct process_handler, list);
			more = (handler->id > preid);
		} else {
			handler = list_entry(handler->list.prev, struct process_handler, list);
			more = (handler->id < preid);
		}
		if (!more) {
			break;
		}
	}
	return ret;
}

int server_msg_register(struct config *conf)
{
	int ret = 0;
	int i = 0;
	size_t len = 0;
	struct node_info *peer;
	struct sockaddr_in addr;
	int addr_len = sizeof(struct sockaddr_in); 
	struct ctrl_header header = {0};

	bzero (&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(conf->server.port);
	addr.sin_addr.s_addr = inet_addr(conf->server.addr);
	
	len = sendto(conf->ctrl_fd, &conf->self->tuple, sizeof(struct tuple), 0, (struct sockaddr *)&addr, addr_len);

	return ret;
}

int server_msg_read(struct config *conf)
{
	int ret = 0;
	int i = 0;
	size_t len = 0;
	struct node_info *peer;
	struct sockaddr_in addr;
	char *saddr;
	int port;
	int addr_len = sizeof(struct sockaddr_in); 
	struct ctrl_header header = {0};
	struct tuple *peers;

	bzero (&addr, sizeof(addr));
	
	len = recvfrom(conf->ctrl_fd, &header, sizeof(header), 0 , (struct sockaddr *)&addr ,&addr_len);
	if (len <= 0) {
		exit(-1);
	}
	
	conf->self->tuple.id = header.did;
  printf("get self ID: %d\n", header.did);
	if (header.num == 0) {
		goto end;
	}
	peers = (struct tuple *)calloc(header.num, sizeof(struct tuple));
	if (!peers) {
		return -1;
	}

	len = recvfrom(conf->ctrl_fd, peers, header.num*sizeof(struct tuple), 0 , (struct sockaddr *)&addr ,&addr_len);
	for (i = 0; i < header.num; i++) {
		struct node_info *peer = (struct node_info *)calloc(1, sizeof(struct node_info));
		memcpy(peer->tuple.addr, peers->addr, 16);
		peer->tuple.port = peers->port;
		peer->tuple.id = peers->id;
		INIT_LIST_HEAD(&peer->list);
		INIT_LIST_HEAD(&peer->macs);
		list_add_tail(&peer->list, &conf->peers);
		peers++;
	}
end:
	return ret;
}

int register_handler(struct process_handler *handler, struct config *conf)
{
	INIT_LIST_HEAD(&handler->list);
	handler->conf = conf;
	list_add_tail(&handler->list, &conf->stack);
	if (conf->first == NULL) {
		conf->first = &handler->list;
	}
	conf->last = &handler->list;
	handler->id = conf->num_handlers;
	conf->num_handlers++;
	return 0;
}

int unregister_handler(struct process_handler *handler, struct config *conf)
{
	//TODO
}

int init_config(struct config *conf)
{
	INIT_LIST_HEAD(&conf->stack);
	INIT_LIST_HEAD(&conf->peers);
	INIT_LIST_HEAD(&conf->macs);
	conf->first = NULL;
	conf->last = NULL;
	conf->type = 0;
	conf->fd.cfd = -1;
	conf->udp_fd = -1;
	conf->self = NULL;
	conf->num_handlers = 0;
}

int init_self(struct config *conf, char *addr, unsigned short port, int type)
{
	int fd = -1;
	struct sockaddr_in saddr; 
	struct node_info *self;

	self = (struct node_info *)calloc(1, sizeof(struct node_info));
	if (self == NULL) {
		exit(-1);
	}
	conf->self = self;
	conf->type = type;

	strcpy(conf->self->tuple.addr, addr);
	conf->self->tuple.port = port;
	conf->self->tuple.type = type;
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
		exit (-1);
	}

	bzero(&saddr, sizeof(saddr));
	saddr.sin_family = AF_INET;  
	saddr.sin_port = htons(conf->self->tuple.port);  
	saddr.sin_addr.s_addr = inet_addr(conf->self->tuple.addr);  
	if (bind(fd, (struct sockaddr *)&saddr, sizeof(saddr))<0){  
		perror("connect");  
		exit(1);  
	}  	

	conf->udp_fd = fd;
}

int init_server_connect(struct config *conf, char *addr, unsigned short port)
{
	int ret = 0;
	int fd = -1;
	struct sockaddr_in srv_addr;

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("socket");
		exit (-1);
	}
	
	bzero(&srv_addr, sizeof(srv_addr));
	srv_addr.sin_family = AF_INET;  
	srv_addr.sin_port = htons(port);  
	srv_addr.sin_addr.s_addr = inet_addr(addr);  
	
	if (connect(fd, (struct sockaddr*)&srv_addr, sizeof(srv_addr)) == -1) {
		perror("connect");
		exit (-1);
	}
	
	conf->ctrl_fd = fd;
	return ret;
}

int main_loop(struct config *conf)
{
	int ret = 0;
	fd_set rd_set;
	int max = 0;
	int type = conf->type;

	if (type == TYPE_EDGE)
		max = conf->fd.cfd;
	if (conf->ctrl_fd > max) {
		max = conf->ctrl_fd;
	}
	if (max < conf->udp_fd) {
		max = conf->udp_fd;
	}

	while(1) {
		int nfds;
		int i;
		FD_ZERO(&rd_set);
		FD_SET(conf->ctrl_fd, &rd_set);
		if (type == TYPE_EDGE) {
			FD_SET(conf->fd.cfd, &rd_set); 
		}
		FD_SET(conf->udp_fd, &rd_set);
	
		nfds = select(max+1, &rd_set, NULL, NULL, NULL);

		for (i = 0;i < nfds; i++) {
			if(FD_ISSET(conf->ctrl_fd, &rd_set)) {
				server_msg_read(conf);
			}
			if(type == TYPE_EDGE && FD_ISSET(conf->fd.cfd, &rd_set)) {
				call_stack(conf, 1);
			}
			if(FD_ISSET(conf->udp_fd, &rd_set)) {
				call_stack(conf, 0);
			}
		}  
	}
	return ret;	
}
