/*
    コンパイル環境: CharctorCode = CP932, Compiler = VC++ 12.
    Copyright (C) 2015, Youichi Minami All Rights Reserved.
*/

#include <list>
#include <vector>
#include <utility>
#include <limits>
#include <string>
#include <iostream>
#include <cstdlib>

static const std::size_t width = 10, height = 10;

struct hand{
    unsigned int x = std::numeric_limits<unsigned int>::max(), y = std::numeric_limits<unsigned int>::max();
    bool pass() const{
        return x == std::numeric_limits<unsigned int>::max() || y ==std::numeric_limits<unsigned int>::max();
    }
};

struct state3_stage{
    enum class state : char{
        null = 0,
        white = 1,
        black = 2
    };

    static const std::size_t table_size = width * height * 2 / (sizeof(unsigned int) * 8) + (width * height * 2 % (sizeof(unsigned int) * 8) > 0 ? 1 : 0);

    // 盤面の状態をビットテーブルで保存する.
    unsigned int bit_table[table_size];

    // 現在の石の個数.
    std::size_t stone_count;

    // 次の盤面を木構造で保持する.
    std::list<std::pair<hand, state3_stage>> subtree;

    // 現在の盤面の価値.
    int avail = 0;

    // この盤面での先手.
    state red;

    // この盤面でのredの適した手.
    hand opt_hand;

    state3_stage *cache;

    state3_stage shallow_copy() const{
        state3_stage r;
        for(int i = 0; i < table_size; ++i){
            r.bit_table[i] = bit_table[i];
        }
        r.stone_count = stone_count;
        r.avail = avail;
        r.red = red;
        r.opt_hand = opt_hand;
        return r;
    }

    // 打つ.
    void apply_red(){
        for(int a = 0; a < table_size; ++a){
            bit_table[a] = cache->bit_table[a];
        }
        stone_count = cache->stone_count;
        {
            auto tmp = std::move(cache->subtree);
            subtree = tmp;
        }
        avail = cache->avail;
        red = cache->red;
        opt_hand = cache->opt_hand;
        cache = nullptr;
    }

    // 戦略木を構築する.
    void compute(int depth){
        if(depth <= 0 || stone_count == width * height){
            short_evaluation(red);
            return;
        }
        if(subtree.empty()){
            subtree = listup_nexthand(red);
        }
        for(auto &i : subtree){
            i.second.red = reverse(red);
            i.second.compute(depth - 1);
        }
        if(subtree.empty()){
            short_evaluation(red);
            opt_hand = hand();
            return;
        }
        int min_max = std::numeric_limits<int>::min();
        for(auto &i : subtree){
            if(min_max < i.second.avail){
                min_max = i.second.avail;
                opt_hand = i.first;
                cache = &i.second;
                avail = i.second.evaluation(reverse(red)) - i.second.evaluation(red);
            }
        }
    };

    // 短絡評価.
    void short_evaluation(state side){
        avail = evaluation(reverse(side)) - evaluation(side);
    }

    // 現在の盤面の評価.
    int evaluation(state side) const{
        int avail = 0;
        for(unsigned int j = 0; j < height; ++j){
            for(unsigned int i = 0; i < width; ++i){
                if((*this)(i, j) == static_cast<int>(side)){
                    // 次が成り立つ様にする.
                    // 角の石の価値 > 辺の石の価値 > それ以外の配置の石の価値.
                    static const int norm_weight = 1;
                    static const int edge_weight = (width - 1) * (height - 1) * norm_weight;
                    static const int kado_weight = edge_weight * 2 * (width + height - 2);
                    if((i == 0 || i == width - 1) && (j == 0 || j == height - 1)){
                        avail += kado_weight;
                    }else if(i == 0 || i == width - 1 || j == 0 || j == height - 1){
                        avail += edge_weight;
                    }else{
                        avail += norm_weight;
                    }
                }
            }
        }
        return avail;
    }

    // 取りうる次の手をリストアップする.
    std::list<std::pair<hand, state3_stage>> listup_nexthand(state stone) const{
        std::list<std::pair<hand, state3_stage>> list;
        for(unsigned int j = 0; j < height; ++j){
            for(unsigned int i = 0; i < width; ++i){
                unsigned int t = (*this)(i, j);
                if(t != static_cast<int>(state::null)){
                    continue;
                }
                auto p = test(i, j, stone);
                if(p.first){
                    std::pair<hand, state3_stage> hs;
                    hs.first.x = i;
                    hs.first.y = j;
                    hs.second = p.second;
                    list.push_back(hs);
                }
            }
        }
        return list;
    }

    // 石を裏返す.
    static state reverse(state s){
        state t;
        if(s == state::white){
            t = state::black;
        }else if(s == state::black){
            t = state::white;
        }
        return t;
    }

    // 指定した位置に石が打てるかどうか.
    // 打てる場合は打った後の盤面を返す.
    std::pair<bool, state3_stage> test(int x, int y, state s) const{
        state r = reverse(s);
        std::size_t count = 0;
        state3_stage ret_stage = shallow_copy();

        if(y - 1 >= 0 && (*this)(x, y - 1) == static_cast<int>(r)){
            std::size_t local_count = 0;
            int y_;
            for(y_ = y - 1; y_ >= 0; --y_){
                if((*this)(x, y_) != static_cast<int>(r)){
                    break;
                }
            }
            if(y_ >= 0 && (*this)(x, y_) == static_cast<int>(s)){
                local_count = y - (y_ + 1);
            }
            if(local_count > 0){
                count += local_count;
                for(std::size_t i = 0; i < local_count; ++i){
                    ret_stage(x, y - i - 1, static_cast<int>(s));
                }
            }
        }

        if(y + 1 < height && (*this)(x, y + 1) == static_cast<int>(r)){
            std::size_t local_count = 0;
            int y_;
            for(y_ = y + 1; y_ < height; ++y_){
                if((*this)(x, y_) != static_cast<int>(r)){
                    break;
                }
            }
            if(y_ < height && (*this)(x, y_) == static_cast<int>(s)){
                local_count = (y_ - 1) - y;
            }
            if(local_count > 0){
                count += local_count;
                for(std::size_t i = 0; i < local_count; ++i){
                    ret_stage(x, y + i + 1, static_cast<int>(s));
                }
            }
        }

        if(x - 1 >= 0 && (*this)(x - 1, y) == static_cast<int>(r)){
            std::size_t local_count = 0;
            int x_;
            for(x_ = x - 1; x_ >= 0; --x_){
                if((*this)(x_, y) != static_cast<int>(r)){
                    break;
                }
            }
            if(x_ >= 0 && (*this)(x_, y) == static_cast<int>(s)){
                local_count = x - (x_ + 1);
            }
            if(local_count > 0){
                count += local_count;
                for(std::size_t i = 0; i < local_count; ++i){
                    ret_stage(x - i - 1, y, static_cast<int>(s));
                }
            }
        }

        if(x + 1 < width && (*this)(x + 1, y) == static_cast<int>(r)){
            std::size_t local_count = 0;
            int x_;
            for(x_ = x + 1; x_ < width; ++x_){
                if((*this)(x_, y) != static_cast<int>(r)){
                    break;
                }
            }
            if(x_ < width && (*this)(x_, y) == static_cast<int>(s)){
                local_count = (x_ - 1) - x;
            }
            if(local_count > 0){
                count += local_count;
                for(std::size_t i = 0; i < local_count; ++i){
                    ret_stage(x + i + 1, y, static_cast<int>(s));
                }
            }
        }

        if(x - 1 >= 0 && y - 1 >= 0 && (*this)(x - 1, y - 1) == static_cast<int>(r)){
            std::size_t local_count = 0;
            int x_, y_;
            for(x_ = x - 1, y_ = y - 1; x_ >= 0 && y_ >= 0; --y_, --x_){
                if((*this)(x_, y_) != static_cast<int>(r)){
                    break;
                }
            }
            if(x_ >= 0 && y_ >= 0 && (*this)(x_, y_) == static_cast<int>(s)){
                local_count = x - (x_ + 1);
            }
            if(local_count > 0){
                count += local_count;
                for(std::size_t i = 0; i < local_count; ++i){
                    ret_stage(x - i - 1, y - i - 1, static_cast<int>(s));
                }
            }
        }

        if(x - 1 >= 0 && y + 1 < height && (*this)(x - 1, y + 1) == static_cast<int>(r)){
            std::size_t local_count = 0;
            int x_, y_;
            for(x_ = x - 1, y_ = y + 1; x_ >= 0 && y_ < height; ++y_, --x_){
                if((*this)(x_, y_) != static_cast<int>(r)){
                    break;
                }
            }
            if(x_ >= 0 && y_ < height && (*this)(x_, y_) == static_cast<int>(s)){
                local_count = x - (x_ + 1);
            }
            if(local_count > 0){
                count += local_count;
                for(std::size_t i = 0; i < local_count; ++i){
                    ret_stage(x - i - 1, y + i + 1, static_cast<int>(s));
                }
            }
        }

        if(x + 1 < width && y + 1 < height && (*this)(x + 1, y + 1) == static_cast<int>(r)){
            std::size_t local_count = 0;
            int x_, y_;
            for(x_ = x + 1, y_ = y + 1; x_ < width && y_ < height; ++y_, ++x_){
                if((*this)(x_, y_) != static_cast<int>(r)){
                    break;
                }
            }
            if(x_ < width && y_ < height && (*this)(x_, y_) == static_cast<int>(s)){
                local_count = (x_ - 1) - x;
            }
            if(local_count > 0){
                count += local_count;
                for(std::size_t i = 0; i < local_count; ++i){
                    ret_stage(x + i + 1, y + i + 1, static_cast<int>(s));
                }
            }
        }

        if(x + 1 < width && y - 1 >= 0 && (*this)(x + 1, y - 1) == static_cast<int>(r)){
            std::size_t local_count = 0;
            int x_, y_;
            for(x_ = x + 1, y_ = y - 1; x_ < width && y_ >= 0; --y_, ++x_){
                if((*this)(x_, y_) != static_cast<int>(r)){
                    break;
                }
            }
            if(x_ < width && y_ < height && (*this)(x_, y_) == static_cast<int>(s)){
                local_count = (x_ - 1) - x;
            }
            if(local_count > 0){
                count += local_count;
                for(std::size_t i = 0; i < local_count; ++i){
                    ret_stage(x + i + 1, y - i - 1, static_cast<int>(s));
                }
            }
        }

        if(count > 0){
            ret_stage(x, y, static_cast<int>(s));
            ++ret_stage.stone_count;
        }
        return std::make_pair(count > 0, ret_stage);
    }

    // 表示する.
    void disp() const{
        auto digit = [](int n){
            int d = 0;
            while(n){
                ++d;
                n /= 10;
            }
            return d;
        };
        int digit_width_num = digit(width);
        for(int i = 0; i < digit_width_num; ++i){
            std::cout << " ";
        }
        for(unsigned int i = 0; i < width; ++i){
            std::cout << static_cast<char>('a' + i);
        }
        std::cout << "\n";
        for(unsigned int j = 0; j < height; ++j){
            int space = digit_width_num - digit(j + 1);
            for(int s = 0; s < space; ++s){
                std::cout << " ";
            }
            std::cout << j + 1;
            for(unsigned int i = 0; i < width; ++i){
                switch(static_cast<state>((*this)(i, j))){
                case state::null:
                    std::cout << "_";
                    break;

                case state::white:
                    std::cout << "*";
                    break;

                case state::black:
                    std::cout << "+";
                    break;
                }
            }
            std::cout << "\n";
        }
    }

    int operator ()(unsigned int x, unsigned int y) const{
        unsigned int i = (y * width + x) * 2;
        return (bit_table[i / (sizeof(unsigned int) * 8)] >> (i % (sizeof(unsigned int) * 8))) & 3;
    }

    void operator ()(unsigned int x, unsigned int y, unsigned int v){
        int i = (y * width + x) * 2;
        if((v & 1) == 1){
            bit_table[i / (sizeof(unsigned int) * 8)] |= (1 << (i % (sizeof(unsigned int) * 8)));
        }else{
            bit_table[i / (sizeof(unsigned int) * 8)] &= ~(1 << (i % (sizeof(unsigned int) * 8)));
        }
        if(((v >> 1) & 1) == 1){
            bit_table[(i + 1) / (sizeof(unsigned int) * 8)] |= (1 << ((i + 1) % (sizeof(unsigned int) * 8)));
        }else{
            bit_table[(i + 1) / (sizeof(unsigned int) * 8)] &= ~(1 << ((i + 1) % (sizeof(unsigned int) * 8)));
        }
    }

    state3_stage(){
        for(std::size_t i = 0; i < table_size; ++i){
            bit_table[i] = 0;
        }
    }
};

std::size_t input(const std::string &str){
    std::string a;
    std::size_t n;
    while(true){
        std::cout << ">> ";
        std::cin >> a;
        n = str.find(a[0]);
        if(n == std::string::npos){
            continue;
        }
        break;
    }
    return n;
}

std::string get_seq(const std::string &str, char first, char last){
    std::string ret;
    std::size_t n = 0;
    while(n < str.size() && str[n] >= first && str[n] <= last){
        ret += str[n];
        ++n;
    }
    return ret;
}

hand coord_in(){
    hand ret;
    std::string a;
    while(true){
        std::cout << ">> ";
        std::cin >> a;
        if(a[0] >= '0' && a[0] <= '9'){
            std::string sub = get_seq(a, '0', '9');
            ret.y = std::atoi(sub.c_str()) - 1;
            ret.x = a[sub.size()] - 'a';
        }else if(a[0] >= 'a' && a[0] <= 'a' + width){
            std::string sub = get_seq(a, 'a', 'a' + width);
            ret.x = sub[0] - 'a';
            ret.y = std::atoi(a.substr(sub.size()).c_str()) - 1;
        }else{
            continue;
        }
        break;
    }
    return ret;
}

void game(){
    int lookahead = 0;
    std::cout << "cpu level: [1] = easy, [2] = normal, [3] = hard.\n";
    lookahead = 2 * (input("123") + 1);

    std::cout << "先手で打つ = [*], 後手で打つ = [+]." << std::endl;
    bool player_first = input("*+") == 0;
    state3_stage::state player_color, cpu_color;
    if(player_first){
        player_color = state3_stage::state::white;
    }else{
        player_color = state3_stage::state::black;
    }
    cpu_color = state3_stage::reverse(player_color);

    state3_stage s;
    s(width / 2 - 1, height / 2 - 1, static_cast<int>(state3_stage::state::white));
    s(width / 2, width / 2, static_cast<int>(state3_stage::state::white));
    s(width / 2 - 1, height / 2, static_cast<int>(state3_stage::state::black));
    s(width / 2, height / 2 - 1, static_cast<int>(state3_stage::state::black));
    s.stone_count = 4;
    s.red = cpu_color;

    if(!player_first){
        s.disp();
        std::cout << "\ncpu: 考え中..." << std::endl;
        s.compute(lookahead);
        std::cout << "cpu: " << s.opt_hand.y + 1 << static_cast<char>('a' + s.opt_hand.x) << std::endl;
        s.apply_red();
    }

    int pass_count = 0;
    while(s.stone_count < width * height && pass_count < 2){
        pass_count = 0;
        std::cout << "\n";
        s.disp();
        std::cout << "\n";

        hand hand_player_side;
        auto list = s.listup_nexthand(player_color);
        if(list.empty()){
            ++pass_count;
            std::cout << ">> pass..." << std::endl;
        }else{
            while(true){
                hand_player_side = coord_in();
                bool f = false;
                for(const auto &i : list){
                    if(i.first.x == hand_player_side.x && i.first.y == hand_player_side.y){
                        f = true;
                        break;
                    }
                }
                if(!f){
                    continue;
                }else{
                    auto s_ = s.test(hand_player_side.x, hand_player_side.y, player_color).second;
                    s = std::move(s_);
                    break;
                }
            }
        }
        
        std::cout << "\n";
        s.disp();
        std::cout << "\ncpu: 考え中..." << std::endl;
        s.compute(lookahead);
        if(s.opt_hand.pass()){
            ++pass_count;
            std::cout << "\ncpu: pass..." << std::endl;
        }else{
            std::cout << "cpu: " << s.opt_hand.y + 1 << static_cast<char>('a' + s.opt_hand.x) << std::endl;
            s.apply_red();
        }
    }

    std::size_t player_side_num = 0, cpu_side_num = 0;
    for(std::size_t y = 0; y < height; ++y){
        for(std::size_t x = 0; x < width; ++x){
            if(s(x, y) == static_cast<int>(player_color)){
                ++player_side_num;
            }else if(s(x, y) == static_cast<int>(cpu_color)){
                ++cpu_side_num;
            }
        }
    }

    std::cout << "\n";
    std::cout << "player: " << player_side_num << "\n";
    std::cout << "cpu   : " << cpu_side_num << "\n\n";
    if(player_side_num > cpu_side_num){
        std::cout << "player win." << std::endl;
    }else{
        std::cout << "cpu win." << std::endl;
    }
}

state3_stage input_stage(const std::string &str){
    state3_stage s;
    for(unsigned int j = 0; j < height; ++j){
        for(unsigned int i = 0; i < width; ++i){
            char c = str[j * width + i];
            s(i, j, static_cast<int>(c == '*' ? state3_stage::state::white : c == '+' ? state3_stage::state::black : state3_stage::state::null));
        }
    }
    return s;
}

int main(){
    game();
    getchar(), getchar();

	return 0;
}

