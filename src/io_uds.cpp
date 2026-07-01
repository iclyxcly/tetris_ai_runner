#include <iostream>
#include <limits>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <functional>
#include "tetris_core.h"
#include "search_amini.h"
#include "ai_zzz.h"
#include "rule_io.h"
#include "random.h"
#include "ai_setting.h"
#include <vector>
#include <nlohmann/json.hpp>
#include <mutex>
#include <string>
#include <atomic>

using JSON = nlohmann::json;
using Bot = m_tetris::TetrisEngine<rule_io::TetrisRule, ai_zzz::IO, search_amini::Search>;
struct BotInstance
{
    std::unique_ptr<Bot> bot;
    std::string buffer;
    BotInstance()
    {
        bot = std::make_unique<Bot>();
        bot->prepare(10, 40);
    }
    void new_game(const JSON &cfg)
    {
        bot->ai_config()->garbage_cap = GARBAGE_CAP;
        bot->ai_config()->multiplier = 1;
        bot->ai_config()->season_2 = cfg["season"].get<int>() == 2;
        bot->ai_config()->lockout = false;

        struct ConfigFlags
        {
            bool is_aspin;
            bool is_amini;
            bool is_tspin;
        };

        static const std::unordered_map<std::string, ConfigFlags> bonus_map = {
            {"all-mini", {false, true, true}},
            {"all-mini+", {false, true, true}},
            {"handheld", {false, true, true}},
            {"mini-only", {false, true, true}},
            {"all", {true, false, false}},
            {"all+", {true, false, false}},
            {"stupid", {true, false, false}},
            {"T-spins", {false, false, true}},
            {"T-spins+", {false, false, true}},
            {"none", {false, false, false}},
        };

        auto &config = *bot->search_config();
        std::string bonus = cfg["bonus"].get<std::string>();
        config.allow_rotate_move = false;
        config.allow_180 = cfg["allow_180"].get<bool>();
        config.allow_D = true;
        config.allow_LR = false;
        config.allow_d = false;
        config.last_rotate = false;
        config.is_20g = false;
        config.allow_immobile_t = bonus.back() == '+' || bonus == "stupid";

        auto it = bonus_map.find(bonus);
        if (it != bonus_map.end())
        {
            config.is_aspin = it->second.is_aspin;
            config.is_amini = it->second.is_amini;
            config.is_tspin = it->second.is_tspin;
        }
        else
        {
            config.is_aspin = false;
            config.is_amini = false;
            config.is_tspin = false;
        }
    }
};

std::mutex srs_ai_lock;
std::unordered_map<uint64_t, BotInstance *> bots;

void print_board(const m_tetris::TetrisMap &map)
{
    printf("Board:\n");
    printf("----");
    for (int i = 0; i < map.width; ++i)
    {
        printf("--");
    }
    printf("\n");
    for (int my = 20; my >= 0; --my)
    {
        printf("|");
        for (int mx = 0; mx < map.width; ++mx)
        {
            if (map.full(mx, my))
            {
                printf("[]");
            }
            else
            {
                printf("  ");
            }
        }
        printf("|\n");
    }
    printf("----");
    for (int i = 0; i < map.width; ++i)
    {
        printf("--");
    }
}

std::string run_ai(BotInstance &bot, const JSON &data)
{
    std::string result;

    auto &srs_ai = bot.bot;

    bool can_hold = data["can_hold"].get<bool>();
    bool can_pc = data["can_pc"].get<bool>();
    char active = data["active"].get<std::string>()[0];
    char hold = data["hold"].get<std::string>()[0];
    int y = data["y"].get<int>();
    char next[32];
    int maxDepth = std::min<int>(data["next_size"].get<int>(), data["next"].get<std::string>().size());
    for (int i = 0; i < maxDepth; ++i)
    {
        next[i] = data["next"].get<std::string>()[i];
    }
    int b2b = data["b2b"].get<int>();
    int combo = data["combo"].get<int>();

    {
        auto &ud = srs_ai->status()->under_attack;
        ud.clear();

        for (const auto& garbage : data["garbageQueue"])
        {
            int lines = std::min<int>(garbage["lines"].get<int>(), std::numeric_limits<uint8_t>::max());
            int steps = std::min<int>(garbage["steps"].get<int>(), std::numeric_limits<uint8_t>::max());
            if (lines > 0)
            {
                ud.push({static_cast<uint8_t>(lines), static_cast<uint8_t>(steps)});
            }
        }
    }

    m_tetris::TetrisMap map(10, 40);
    for (size_t d = 0; d < 23; ++d)
    {
        map.row[d] = data["field"][d].get<uint32_t>();
    }
    for (int my = 0; my < map.height; ++my)
    {
        for (int mx = 0; mx < map.width; ++mx)
        {
            if (map.full(mx, my))
            {
                map.top[mx] = map.roof = my + 1;
                map.row[my] |= 1 << mx;
                ++map.count;
            }
        }
    }

    srs_ai->ai_config()->pc = can_pc;
    srs_ai->status()->max_combo = 0;
    srs_ai->status()->attack = 0;
    srs_ai->status()->b2bcnt = b2b;

    srs_ai->memory_limit(512ull << 20);
    srs_ai->status()->death = 0;
    srs_ai->status()->combo = combo;
    srs_ai->status()->map_rise = 0;
    srs_ai->status()->like = 0;
    srs_ai->status()->value = 0;

    m_tetris::TetrisBlockStatus status(active, 3, y, 0);
    m_tetris::TetrisNode const *node = srs_ai->get(status);
    if (can_hold)
    {
        auto run_result = srs_ai->run_hold(map, node, hold, true, next, maxDepth, time_t(50));
        if (run_result.change_hold)
        {
            result += "v";
            if (run_result.target != nullptr)
            {
                std::vector<char> ai_path = srs_ai->make_path(srs_ai->context()->generate(run_result.target->status.t), run_result.target, map);
                for (auto &c : ai_path)
                {
                    result += c;
                }
            }
        }
        else
        {
            if (run_result.target != nullptr)
            {
                std::vector<char> ai_path = srs_ai->make_path(node, run_result.target, map);
                for (auto &c : ai_path)
                {
                    result += c;
                }
            }
        }
    }
    else
    {
        auto run_result = srs_ai->run(map, node, next, maxDepth, time_t(50));
        if (run_result.target != nullptr)
        {
            std::vector<char> ai_path = srs_ai->make_path(node, run_result.target, map);
            for (auto &c : ai_path)
            {
                result += c;
            }
        }
    }
    result += 'V';
    return result;
}

void new_bot(int bot_id)
{
    std::lock_guard<std::mutex> lock(srs_ai_lock);
    if (bots.find(bot_id) != bots.end())
    {
        throw std::runtime_error("Bot exists [new_bot]");
    }
    bots[bot_id] = new BotInstance();
}

void end_bot(int bot_id)
{
    std::lock_guard<std::mutex> lock(srs_ai_lock);
    auto it = bots.find(bot_id);
    if (it != bots.end())
    {
        delete it->second;
        bots.erase(it);
    }
}

void new_game(int bot_id, const JSON &data)
{
    std::lock_guard<std::mutex> lock(srs_ai_lock);
    auto it = bots.find(bot_id);
    if (it != bots.end())
    {
        it->second->new_game(data);
    }
}

std::string TetrisAI(int bot_id, const JSON &data)
{
    BotInstance *bot;
    {
        std::lock_guard<std::mutex> lock(srs_ai_lock);
        auto it = bots.find(bot_id);
        if (it == bots.end())
            return "V";
        bot = it->second;
    }

    bot->buffer = run_ai(*bot, data);

    return bot->buffer;
}

struct Message
{
    int bot_id;
    std::string command;
    JSON data;
    bool success = false;

    static Message parse(const std::string &src)
    {
        Message msg;
        std::cout << "Received: " + src << std::endl;
        const JSON data = JSON::parse(src);
        try
        {
            if (data.contains("bot_id") && data["bot_id"].is_number())
            {
                msg.bot_id = data["bot_id"];
            }
            else
            {
                throw std::invalid_argument("Missing or invalid 'bot_id'");
            }

            if (data.contains("command") && data["command"].is_string())
            {
                msg.command = data["command"];
            }
            else
            {
                throw std::invalid_argument("Missing or invalid 'command'");
            }

            if (data.contains("data") && data["data"].is_object())
            {
                msg.data = data["data"];
            }
            msg.success = true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error parsing JSON: " << e.what() << std::endl;
        }
        return msg;
    }

    static JSON pack(const Message &src)
    {
        JSON json;
        try
        {
            json["bot_id"] = src.bot_id;
            json["command"] = src.command;
            json["data"] = src.data;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error packing Message to JSON: " << e.what() << std::endl;
            throw;
        }
        return json;
    }

    Message(const int &bot_id, const std::string &command, const JSON &data) : bot_id(bot_id), command(command), data(data) {}
    Message() {}
};

class UdsServer
{
public:
    using message_handler = std::function<std::string(const std::string &)>;

    explicit UdsServer(const char *socket_path = "/tmp/tetris_ai")
        : path_(socket_path), running_(false), server_fd_(-1)
    {
        ::unlink(path_);
    }

    ~UdsServer()
    {
        stop();
        ::unlink(path_);
    }

    void set_handler(message_handler handler)
    {
        handler_ = std::move(handler);
    }

    bool start(int backlog = 5)
    {
        std::cout << "Listening..." << std::endl;
        server_fd_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (server_fd_ < 0)
            return false;

        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        std::strncpy(addr.sun_path, path_, sizeof(addr.sun_path) - 1);

        if (::bind(server_fd_, (sockaddr *)&addr, sizeof(addr)) < 0)
            return false;
        if (::listen(server_fd_, backlog) < 0)
            return false;

        running_.store(true);
        while (running_.load())
        {
            int client_fd = ::accept(server_fd_, nullptr, nullptr);
            if (client_fd < 0)
                continue;
            std::thread(&UdsServer::handle_client, this, client_fd).detach();
        }
        return true;
    }

    void stop()
    {
        running_.store(false);
        if (server_fd_ >= 0)
        {
            ::shutdown(server_fd_, SHUT_RDWR);
            ::close(server_fd_);
            server_fd_ = -1;
        }
    }

private:
    void handle_client(int fd)
    {
        constexpr size_t buf_size = 4096;
        std::vector<char> buf(buf_size);

        std::string leftover;
        while (true)
        {
            ssize_t n = ::read(fd, buf.data(), buf_size);
            if (n <= 0)
                break;

            leftover.append(buf.data(), n);
            size_t pos;
            while ((pos = leftover.find('\n')) != std::string::npos)
            {
                std::string line = leftover.substr(0, pos);
                leftover.erase(0, pos + 1);
                std::string reply = handler_(line);
                if (!reply.empty())
                {
                    reply.push_back('\n');
                    ssize_t w = ::send(fd,
                                    reply.data(),
                                    reply.size(),
                                    MSG_NOSIGNAL);
                    if (w <= 0 && errno == EPIPE)
                    {
                        end_bot(Message::parse(line).bot_id);
                        close(fd);
                        return;
                    }
                }
            }
        }
        ::close(fd);
    }

    const char *path_;
    message_handler handler_;
    std::atomic<bool> running_;
    int server_fd_;
};

int main()
{
    std::string path = std::getenv("HOME");
    path += "/tetris_ai";
    UdsServer server(path.c_str());
    server.set_handler(
        [&](const std::string &msg)
        {
            Message content = Message::parse(msg);

            if (content.command == "create")
            {
                new_bot(content.bot_id);
            }
            else if (content.command == "prepare")
            {
                new_game(content.bot_id, content.data);
            }
            else if (content.command == "dismiss")
            {
                end_bot(content.bot_id);
            }
            else if (content.command == "move")
            {
                std::string res = TetrisAI(content.bot_id, content.data);
                return Message::pack({content.bot_id, content.command, res}).dump();
            }
            return std::string("");
        });
    server.start();
    std::cin.get();
    return 0;
}