#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "hotpocket_contract.h"

// gcc echo_contract.c -o echo_contract

void store_timestamp(const uint64_t timestamp);
void process_user_message(const struct hp_user *user, const void *buf, const uint32_t len);
void contract_handler(const struct hp_contract_context *ctx, const bool is_readonly);
void fallback_mode_handler(const struct hp_contract_context *ctx);

int main(int argc, char **argv)
{
    // Initialize the HotPocket contract context for this execution.
    if (hp_init_contract() == -1)
        return 1;

    const struct hp_contract_context *ctx = hp_get_context();

    // Get the contract execution mode.
    const enum EXECUTION_MODE exec_mode = ctx->get_exec_mode(ctx->mode);

    // Defining the relevant behaviour based on the execution mode.
    switch (exec_mode)
    {
    case CONSENSUS:
        contract_handler(ctx, false);
        break;

    case CONSENSUS_FALLBACK:
        fallback_mode_handler(ctx);
        break;

    case READ_REQUEST:
        contract_handler(ctx, true);
        break;

    default:
        break;
    }

    hp_deinit_user_input_mmap();
    hp_deinit_contract();
    return 0;
}

void store_timestamp(const uint64_t timestamp)
{
    int fd = open("exects.txt", O_RDWR | O_CREAT | O_APPEND, 0644);
    if (fd > 0)
    {
        char tsbuf[20];
        memset(tsbuf, 0, 20);
        sprintf(tsbuf, "%lu\n", timestamp);
        struct iovec vec[2] = {{(void *)"ts:", 3}, {(void *)tsbuf, strlen(tsbuf)}};
        writev(fd, vec, 2);
        close(fd);
    }
}

void process_user_message(const struct hp_user *user, const void *buf, const uint32_t len)
{
    if (strncmp((const char *)buf, "ts", 2) == 0)
    {
        int fd = open("exects.txt", O_RDONLY);
        if (fd > 0)
        {
            struct stat st;
            if (fstat(fd, &st) != -1)
            {
                char tsbuf[st.st_size];
                if (read(fd, tsbuf, st.st_size) > 0)
                {
                    for (int i = 0; i < st.st_size; i++)
                    {
                        if (tsbuf[i] == '\n' || tsbuf[i] == 0)
                            tsbuf[i] = ' ';
                    }
                    hp_write_user_msg(user, tsbuf, st.st_size - 1);
                }
            }
            close(fd);
        }
    }
    else
    {
        struct iovec vec[2] = {{(void *)"Echoing: ", 9}, {(void *)buf, len}};
        hp_writev_user_msg(user, vec, 2);
    }
}

void contract_handler(const struct hp_contract_context *ctx, const bool is_readonly)
{
    if (!is_readonly)
        store_timestamp(ctx->timestamp);

    // Read and process all user inputs from the mmap.
    const void *input_mmap = hp_init_user_input_mmap();

    // Iterate through all users.
    for (size_t u = 0; u < ctx->users.count; u++)
    {
        const struct hp_user *user = &ctx->users.list[u];

        // Iterate through all inputs from this user.
        for (size_t i = 0; i < user->inputs.count; i++)
        {
            const struct hp_user_input input = user->inputs.list[i];

            // Instead of mmap, we can also read the inputs from 'ctx->users.in_fd' using file I/O.
            // However, using mmap is recommended because user inputs already reside in memory.
            const uint8_t *buf = (uint8_t *)input_mmap + input.offset;

            process_user_message(user, buf, input.size);
        }
    }

    // NPL message send example:
    // hp_write_npl_msg("Hello!", 6);

    // NPL message receive example:
    // // Allocate buffers for received message.
    // char sender[HP_KEY_SIZE];
    // char *msg = malloc(HP_NPL_MSG_MAX_SIZE);
    // // Wait for 200ms for incoming message. We will receive our own message as well if we are part of unl.
    // const int len = hp_read_npl_msg(msg, sender, 200);
    // if (len > 0)
    //     printf("Received %.*s from %.*s", len, msg, HP_KEY_SIZE, sender);
    // free(msg);

    // Peers update example:
    // const char *add_peers[2] = {"localhost:4444", "localhost:3333"};
    // hp_update_peers(add_peers, 2, NULL, 0);

    // Config update example:
    // struct hp_config *config = hp_get_config();
    // hp_set_config_string(&config->version, "2.0", 4);
    // config->round_limits.user_input_bytes = 1024;
    // struct hp_public_key new_unl[2] = {{"ed726f9f536904b125bdca10bbdd1e66591b274799b92ac8bcfc75bf45d7da4c0f"},
    //                                  {"ed3e63992d62804ea0c182e5b22fe43c4b652fbbf068ec7520f3020f4c3771416a"}};
    // hp_set_config_unl(config, new_unl, 2);
    // hp_update_config(config);
    // hp_free_config(config);
}

void fallback_mode_handler(const struct hp_contract_context *ctx)
{
    printf("Fallback mode: Non consensus execution count: %u\n", ctx->non_consensus_rounds);

    // NPL message send example:
    hp_write_npl_msg("Hello!", 6);

    // NPL message receive example:
    // Allocate buffers for received message.
    char sender[HP_PUBLIC_KEY_SIZE];
    char *msg = malloc(HP_NPL_MSG_MAX_SIZE);
    // Wait for 200ms for incoming message. We will receive our own message as well if we are part of unl.
    const int len = hp_read_npl_msg(msg, sender, 200);
    if (len > 0)
        printf("Received %.*s from %.*s\n", len, msg, HP_PUBLIC_KEY_SIZE, sender);
    free(msg);
}
