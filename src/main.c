/**
 * @file main.c
 * @author Paulo Santos (prms@ic.ufal.br)
 * @brief STNP with Zbus.
 * @version 0.1
 * @date 14/10/2025
 *
 * Atividade 4 - SNTP + Zbus
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/sntp.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/sys/timeutil.h>


LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

struct data {
    struct tm timestamp;
};

struct endpoint {
    struct sockaddr addr;
    socklen_t len;
};

K_SEM_DEFINE(got_ip_sem, 0, 1);
static struct net_mgmt_event_callback ip_cb;
bool is_wifi_conn = false;

static void ip_event_handler(struct net_mgmt_event_callback* cb, const uint64_t mgmt_event,
                             struct net_if* iface) {
    if (mgmt_event == NET_EVENT_IPV4_ADDR_ADD) {
        char ip[NET_IPV4_ADDR_LEN];
        const struct net_if_config* cfg = net_if_get_config(iface);

        if (cfg && cfg->ip.ipv4) {
            net_addr_ntop(AF_INET, &cfg->ip.ipv4->unicast[0].ipv4.address.in_addr, ip,
                          sizeof(ip));
            LOG_INF("DHCP OK: %s", ip);
            k_sem_give(&got_ip_sem);
        }
    }
}

static int wifi_connect_now(const uint8_t* ssid, const uint8_t* psk) {
    struct net_if* iface = net_if_get_default();
    struct wifi_connect_req_params req_params = {0};

    req_params.ssid = ssid;
    req_params.ssid_length = strlen((char*) ssid);
    req_params.psk = psk;
    req_params.psk_length = strlen((char*) psk);
    req_params.security = WIFI_SECURITY_TYPE_PSK;
    req_params.channel = WIFI_CHANNEL_ANY;

    const int ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &req_params, sizeof(req_params));
    if (ret) {
        LOG_ERR("Failed NET_REQUEST_WIFI_CONNECT (%d)", ret);
        return ret;
    }

    LOG_INF("Connecting on AP \"%s\" ...", ssid);
    return 0;
}

int init(void) {
    LOG_INF("Starting Wi-Fi");

    net_mgmt_init_event_callback(&ip_cb, ip_event_handler, NET_EVENT_IPV4_ADDR_ADD);
    net_mgmt_add_event_callback(&ip_cb);

    if (wifi_connect_now(CONFIG_WIFI_SSID, CONFIG_WIFI_PASSWD) != 0) {
        return -EIO;
    }

    if (k_sem_take(&got_ip_sem, K_SECONDS(30)) != 0) {
        LOG_ERR("DHCP Timeout");
        return -ETIMEDOUT;
    }

    struct zsock_addrinfo hints = {0},* res = NULL;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    const char* hostname_test = "google.com";

    const int ret = zsock_getaddrinfo(hostname_test, "80", &hints, &res);
    if (ret) {
        LOG_ERR("DNS failed (%d)", ret);

        return -1;
    }

    char ip_buffer[NET_IPV4_ADDR_LEN];
    const struct sockaddr_in* addr = (struct sockaddr_in*) res->ai_addr;

    net_addr_ntop(AF_INET, &addr->sin_addr, ip_buffer, sizeof(ip_buffer));

    LOG_INF("DNS OK: %s -> %s", ip_buffer, hostname_test);
    zsock_freeaddrinfo(res);

    is_wifi_conn = true;

    return 0;
}

ZBUS_CHAN_DEFINE(time_channel, struct data, NULL, NULL, ZBUS_OBSERVERS(log_subs, app_subs),
                 ZBUS_MSG_INIT(.timestamp = 0));

K_THREAD_STACK_DEFINE(thread_sntp_stack, 1024);
K_THREAD_STACK_DEFINE(thread_logger_stack, 1024);
K_THREAD_STACK_DEFINE(thread_app_stack, 1024);

static struct k_thread thread_sntp_data;
static struct k_thread thread_logger_data;
static struct k_thread thread_app_data;

static struct endpoint sntp_endpoint;
static struct sntp_time s_time;
static K_SEM_DEFINE(sntp_async_received, 0, 1);

static void sntp_service_handler(struct net_socket_service_event* pev);

NET_SOCKET_SERVICE_SYNC_DEFINE_STATIC(service_sntp_async, sntp_service_handler, 1);

SYS_INIT(init, APPLICATION, 50);

static void sntp_service_handler(struct net_socket_service_event* pev) {
    const int err = sntp_read_async(pev, &s_time);
    if (err) {
        LOG_ERR("[SNTP] failed to read SNTP response (%d)", err);
        return;
    }

    k_sem_give(&sntp_async_received);
}

void logger_thread(void* arg1, void* arg2, void* arg3) {
    char date_time[32] = {0};

    static struct tm internal_clock = {0};
    const struct zbus_channel* ch;
    struct data msg;

    LOG_INF("[LOGGER] Starting service");

    while (true) {
        int err = zbus_sub_wait(&log_subs, &ch, K_FOREVER);
        if (err) {
            LOG_WRN("[LOGGER] error while waiting channel notification: %d", err);
            continue;
        }

        err = zbus_chan_read(ch, &msg, K_FOREVER);
        if (err) {
            LOG_WRN("[LOGGER] error while reading channel msg: %d", err);
            continue;
        }

        internal_clock = msg.timestamp;

        strftime(date_time, 30, "%a %Y-%m-%d %H:%M:%S %Z", &internal_clock);
        LOG_INF("[LOGGER] Internal clock updated: %s", date_time);
    }
}

void app_thread(void* arg1, void* arg2, void* arg3) {
    bool init_ts = false;
    char date_time[32] = {0};
    char format[64];
    snprintf(format, sizeof(format), "%%a %%Y-%%m-%%d %%H:%%M:%%S %%Z%+d", CONFIG_UTC_OFFSET);

    const struct zbus_channel* ch;
    static struct tm last_timestamp = {0};
    struct data msg;

    LOG_INF("[APP] Starting service");

    while (true) {
        int err = zbus_sub_wait(&app_subs, &ch, K_FOREVER);
        if (err) {
            LOG_WRN("[APP] error while waiting channel notification: %d", err);
            continue;
        }

        err = zbus_chan_read(ch, &msg, K_FOREVER);
        if (err) {
            LOG_WRN("[APP] error while reading channel msg: %d", err);
            continue;
        }

        strftime(date_time, 30, format, &last_timestamp);
        LOG_DBG("[APP] Last execution time: %s", date_time);

        strftime(date_time, 30, format, &msg.timestamp);
        LOG_DBG("[APP] Now execution time: %s", date_time);

        if (!init_ts) {
            last_timestamp = msg.timestamp;
            init_ts = true;
        }

        int64_t ta = timeutil_timegm64(&last_timestamp);
        int64_t tb = timeutil_timegm64(&msg.timestamp);

        LOG_INF("[APP] Time execution interval: %" PRId64 "s", tb - ta);

        last_timestamp = msg.timestamp;
    }
}

void sntp_thread(void* arg1, void* arg2, void* arg3) {
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    char format[64];
    snprintf(format, sizeof(format), "%%a %%Y-%%m-%%d %%H:%%M:%%S %%Z%+d", CONFIG_UTC_OFFSET);

    struct endpoint* sntp_endpoint_ = arg1;
    struct sntp_ctx ctx;

    int err = sntp_init_async(&ctx, &sntp_endpoint_->addr, sntp_endpoint_->len,
                              &service_sntp_async);
    if (err) {
        LOG_ERR("Failed to init SNTP, ctx: %d", err);
        sntp_close(&ctx);

        return;
    }

    LOG_INF("Starting SNTP Service");

    while (true) {
        struct tm time_utc;

        k_sem_reset(&sntp_async_received);
        err = sntp_send_async(&ctx);

        if (err) {
            LOG_WRN("[SNTP] Failed to send SNTP query (%d)", err);
            continue;
        }

        err = k_sem_take(&sntp_async_received, K_MSEC(1000));
        if (err) {
            LOG_WRN("[SNTP] response timed out (%d)", err);
            continue;
        }

        const struct timespec ts = {
            .tv_sec = (time_t) s_time.seconds,
            .tv_nsec = (long) (((uint64_t) s_time.fraction * 1000000000ULL) >> 32)
        };

        sys_clock_settime(1, &ts);

        time_t local_sec = (time_t) s_time.seconds + (CONFIG_UTC_OFFSET * 3600);
        gmtime_r(&local_sec, &time_utc);

        char date_time[32];
        strftime(date_time, 30, format, &time_utc);

        LOG_INF("[SNTP] Localtime updated: %s", date_time);

        struct data msg = {.timestamp = time_utc};
        err = zbus_chan_pub(&time_channel, &msg, K_NO_WAIT);
        if (err) {
            LOG_WRN("[SNTP] failed to publish in channel");
        }

        const uint32_t sleep_time = (k_cycle_get_32() % 5000) + 500;

        k_sleep(K_MSEC(sleep_time));
    }
}

ZBUS_SUBSCRIBER_DEFINE(app_subs, 4);
ZBUS_SUBSCRIBER_DEFINE(log_subs, 4);

int main(void) {
    if (!is_wifi_conn) {
        LOG_INF("Unable to connect to the WIFI.");
        return -1;
    }

    char ip_buffer[NET_IPV4_ADDR_LEN];

    struct zsock_addrinfo hints;
    struct zsock_addrinfo* res;

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    k_sleep(K_SECONDS(2));

    const int err = zsock_getaddrinfo(CONFIG_SNTP_HOSTNAME, "123", &hints, &res);
    if (err) {
        LOG_ERR("Failed to get hostname info");
        return -1;
    }

    net_addr_ntop(AF_INET, res->ai_addr, ip_buffer, sizeof(ip_buffer));

    LOG_INF("DNS SNTP OK: %s -> %s", ip_buffer, CONFIG_SNTP_HOSTNAME);

    sntp_endpoint.len = res->ai_addrlen;
    memcpy(&sntp_endpoint.addr, res->ai_addr, res->ai_addrlen);

    zsock_freeaddrinfo(res);

    k_thread_create(&thread_sntp_data, thread_sntp_stack,
                    K_THREAD_STACK_SIZEOF(thread_sntp_stack), sntp_thread, &sntp_endpoint, NULL,
                    NULL, K_PRIO_PREEMPT(4), 0, K_NO_WAIT);

    k_thread_create(&thread_logger_data, thread_logger_stack,
                    K_THREAD_STACK_SIZEOF(thread_logger_stack), logger_thread, NULL, NULL, NULL,
                    K_PRIO_PREEMPT(3), 0, K_NO_WAIT);

    k_thread_create(&thread_app_data, thread_app_stack, K_THREAD_STACK_SIZEOF(thread_app_stack),
                    app_thread, NULL, NULL, NULL, K_PRIO_PREEMPT(3), 0, K_NO_WAIT);

    LOG_INF("System started successfully");

    return 0;
}
