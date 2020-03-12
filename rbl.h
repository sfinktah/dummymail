extern char *rbl_ip;

// char ** explode_mod2(char seperator, char *data, int *num)
// void hex_dump(unsigned char *buf, int len) {
// void vhex(char *str, void *buf, int len) {
// dns_reply(char *reply, int len)
// char *dns_query(char *fqdn, size_t *len) {

int rbl_check_ip(char *ip);
int rbl_init(char *_rbl_ip);

