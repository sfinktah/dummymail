int do_parse_conf_file(pth_attr_t attr, const char* argv0);

struct Address {
    char ip[INET6_ADDRSTRLEN];
    int  port;
};

