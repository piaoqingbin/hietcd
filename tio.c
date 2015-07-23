#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "hietcd.h"
#include "log.h"
#include "response.h"

static inline void etcd_node_print(etcd_node *node, const char *node_name, uint32_t depth)
{
    if (node == NULL) return;
    printf("\n");
    printf("%*sresp_%s_key:%s\n", depth*2, " ", node_name, node->key);
    printf("%*sresp_%s_value:%s\n", depth*2, " ", node_name, node->value);
    printf("%*sresp_%s_isdir:%d\n", depth*2, " ", node_name, node->isdir);
    printf("%*sresp_%s_count:%llu\n", depth*2, " ", node_name, node->ccount);
    printf("%*sresp_%s_created_index:%lld\n", depth*2, " ", node_name, node->cidx);
    printf("%*sresp_%s_modified_index:%lld\n", depth*2, " ", node_name, node->midx);
    printf("%*sresp_%s_ttl:%d\n", depth*2, " ", node_name, node->ttl);
    printf("%*sresp_%s_expiration:%s\n", depth*2, " ", node_name, node->expr);
    printf("%*sresp_%s_sibling:%d\n", depth*2, " ", node_name, node->snode ? 1 : 0);
    printf("%*sresp_%s_child:%d\n", depth*2, " ", node_name, node->cnode ? 1 : 0);
    if (node->cnode)
        etcd_node_print(node->cnode, node_name, depth + 1); 
    if (node->snode)
        etcd_node_print(node->snode, node_name, depth); 
}

void etcd_response_print(etcd_response *resp)
{
    if (resp == NULL) return;
    printf("resp_http_code:%d\n", resp->hcode);
    printf("resp_curl_code:%d\n", resp->ccode);
    printf("resp_etcd_index:%lld\n", resp->idx);
    printf("resp_raft_index:%lld\n", resp->ridx);
    printf("resp_raft_term:%lld\n", resp->rterm);
    printf("resp_errcode:%d\n", resp->errcode);
    printf("resp_errmsg:%s\n", resp->errmsg);
    printf("resp_cluster:%s\n", resp->cluster);
    printf("resp_action:%s\n", resp->action);
    printf("resp_node:%d\n", resp->node ? 1 : 0);
    printf("resp_pnode:%d\n", resp->pnode ? 1 : 0);
    printf("resp_data:%s\n", resp->data);
    etcd_node_print(resp->node, "node", 1);
    etcd_node_print(resp->pnode, "pnode", 1);
}


void proc(etcd_client *client, etcd_response *resp, void *data)
{
//    etcd_response_print(resp);
    printf("done!\n");
}

int main(void) {

    etcd_client *client = etcd_client_create(); 
//    etcd_set_log_level(ETCD_LOG_LEVEL_DEBUG);
    client->servers[0] = "http://10.69.56.43:2379";
    etcd_set_response_proc(client, proc, "test_data_haha");

    etcd_start_io_thread(client);

    sleep(1);

    int i;
    for (i = 1; i < 2; i++) {
        etcd_amkdir(client, "/test/key1", i*100);
        etcd_amkdir(client, "/test/key2", i*100);
    }

//    sleep(3);

    const char *v1 = "hahah_v1";
    etcd_aset(client, "/test/key1/tn1", v1, sizeof(v1), 100);


    sleep(10);

    etcd_client_destroy(client);


    /*
    while (1) {
        fprintf(stderr, "main running...\n"); 
        sleep(1);
    }
    */

}
