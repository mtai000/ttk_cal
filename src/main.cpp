#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <mutex>
#include <thread>
#include <iomanip>
#include <random>
#include <algorithm>
#include <iomanip>
#include <map>
#ifdef _WIN32
#include "windows.h"
#else
#include <locale>
#endif

inline double random_double() {
	static std::uniform_real_distribution<double> distribution(0.0, 1.0);
	static std::mt19937 generator;
	return distribution(generator);
}

struct TTK {
	std::string name;
	int count;
};

struct Setting {
	int lv_bullet;
	int lv_helmet;
	int lv_armor;
	float dur_helmet;
	float dur_armor;
};
struct Player : public Setting {
	float hp = 100;
	Player(Setting s) {
		this->hp = 100;
		this->lv_bullet = s.lv_bullet;
		this->lv_armor = s.lv_armor;
		this->lv_helmet = s.lv_helmet;
		this->dur_armor = s.dur_armor;
		this->dur_helmet = s.dur_helmet;
	}
};


std::mutex mutex;
Setting setting;
unsigned int max_times;


const float k_bullet[4][4] = { 0.9,0.9,0.5,0.4,
							   1.0,1.0,1.0,1.0,
							   1.1,1.1,1.1,1.1,
							   1.2,1.2,1.2,1.2 };

const int possibility[4] = { 912,1528 + 912,2716 + 1528 + 912,4844 + 2716 + 1528 + 912 };
const float k_dam[2] = { 0.9,0.4 };
struct Gun {
	std::string name;
	float dam_head;
	float dam_chest;
	float dam_abd;
	float dam_libms;
	float armor_dam;
	float rate_of_fire;
	int count;
	std::map<int, int> map_shot;

	void show() {
		std::cout << "名字    : " << name << std::endl;
		std::cout << "爆头伤害: " << dam_head << std::endl;
		std::cout << "胸口伤害: " << dam_chest << std::endl;
		std::cout << "腹部伤害: " << dam_abd << std::endl;
		std::cout << "四肢伤害: " << dam_libms << std::endl;
		std::cout << "护甲伤害: " << armor_dam << std::endl;
		std::cout << "射速    : " << rate_of_fire << std::endl;
		std::cout << "=========================" << std::endl;
	}

	void show_ttk() {
		float avg_shots = static_cast<float>(count) / max_times;
		float ttk = (avg_shots - 1.f) * 60.f / rate_of_fire;
		std::cout << std::setw(14) << name;
		printf(",\t%.2f,\t%.2f\n", avg_shots, ttk);
	}

	void show_detail() {
		int len1 = 6, len2 = 4, len3 = 8;
		std::cout << "名字    : " << name << std::endl;
		std::cout << std::setw(len1) << "击杀枪数,";
		std::cout << std::setw(len2) << "ttk,";
		std::cout << std::setw(len3) << "概率" << std::endl;
		unsigned int sum = 0;
		for (const auto& [shot, count] : map_shot) {
			std::cout << std::setw(len1) << shot << ",";
			std::cout << std::setw(len2) << std::fixed << std::setprecision(2) << (shot - 1) / rate_of_fire * 60 << ",";
			std::cout << std::right << std::setw(len3) << std::fixed << std::setprecision(2) << 1.f*count/max_times * 100  << std::endl;
			sum += count;
		}

		if (sum != max_times) 
		{
			std::cerr << "error!!!!! Missing sim data" << std::endl; 
		}
	}
};
std::vector<std::string> split(const std::string& s, char delimiter) {
	std::vector<std::string> tokens;
	std::string token;
	std::istringstream tokenStream(s);
	while (std::getline(tokenStream, token, delimiter)) {
		tokens.push_back(token);
	}
	return tokens;
}


void inputGunData(std::vector<Gun>& vg) {
	std::ifstream file("data.csv");
	std::string line;
	while (std::getline(file, line))
	{
		if (line.empty()) continue;
		auto tokens = split(line, ',');
		if (tokens.size() != 7) {
			std::cout << "error line,skip it: \t" << line << std::endl;
			continue;
		}

		try {
			Gun gun;
			gun.name = tokens[0];
			gun.dam_head = std::stof(tokens[1]);
			gun.dam_chest = std::stof(tokens[2]);
			gun.dam_abd = std::stof(tokens[3]);
			gun.dam_libms = std::stof(tokens[4]);
			gun.armor_dam = std::stof(tokens[5]);
			gun.rate_of_fire = std::stof(tokens[6]);
			gun.count = 0;
			//gun.printf();
			vg.push_back(gun);
		}
		catch (const std::exception& e) {
			std::cout << "转换错误: " << e.what() << "，行内容: " << line << std::endl;
		}
	}
}

void cal(float dam, float armor_dam, float& dur, float& hp, int lv_bullet, int lv_armor) {
	float pene;
	if (lv_armor > lv_bullet) {
		pene = 0.f;
	}
	else if (lv_armor == lv_bullet) {
		pene = 0.5f;
	}
	else if (lv_armor == lv_bullet - 1) {
		pene = 0.75f;
	}
	else {
		pene = 1.f;
	}

	float k_armor_dam = k_bullet[lv_bullet - 3][lv_armor - 3];

	if (dur > 0) {
		if (dur < armor_dam * k_armor_dam) {
			float k_overflow_dam = 1 - dur / armor_dam / k_armor_dam;
			float overflow_dam = dam * k_overflow_dam;
			hp -= overflow_dam;
			hp -= dam * pene;
			dur = 0;
		}
		else {
			dur -= armor_dam * k_armor_dam;
			hp -= dam * pene;
		}
	}
	else {
		hp -= dam;
	}

}

void sim(Gun& gun, int start, int end) {
	int count = 0;
	std::map<int, int> map_shot;
	for (int i = start; i < end; i++) {
		Player player(setting);
		int shot = 0;
		for (; player.hp > 0; shot++) {
			double k = random_double() * 10000;
			//k = 900.f;
			if (k < possibility[0]) { //头
				cal(gun.dam_head, gun.armor_dam, player.dur_helmet, player.hp, player.lv_bullet, player.lv_helmet);
			}
			else if (k < possibility[1]) { //胸
				cal(gun.dam_chest, gun.armor_dam, player.dur_armor, player.hp, player.lv_bullet, player.lv_armor);
			}
			else if (k < possibility[2]) { //腹
				cal(gun.dam_abd, gun.armor_dam, player.dur_armor, player.hp, player.lv_bullet, player.lv_armor);
			}
			else if (k < possibility[3]) { //四肢
				player.hp -= gun.dam_libms;
			}
		}
		count += shot;
		mutex.lock();
		gun.map_shot[shot]++;
		mutex.unlock();
	}
	mutex.lock();
	gun.count += count;
	mutex.unlock();
}

void simulate(Gun& gun) {
	auto num_threads = std::thread::hardware_concurrency();
	num_threads = 1;
	std::vector<std::thread> threads;
	const int chunk_size = max_times / num_threads;
	for (int i = 0; i < num_threads;i++) {
		int start = i * chunk_size;
		int end = (i == num_threads - 1) ? max_times : start + chunk_size;
		//threads.emplace_back(&sim,std::ref(gun),start,end);
		threads.push_back(std::thread(sim, std::ref(gun), start, end));
	}
	for (auto& t : threads) t.join();
}

void verify_valid(int lv) {
	if (lv < 3 || lv > 6) { // 添加参数有效性检查
		throw std::invalid_argument("等级无效");
	}
}

int main(int argc, char** argv) {
#ifdef _WIN32
	SetConsoleOutputCP(CP_UTF8);
#else
	std::locale::global(std::locale("en_US.UTF-8"));
#endif

	std::vector<Gun> vg;
	inputGunData(vg);

	if (argc != 7) {
		std::cout << "ttk_cal.exe 子弹等级 头甲等级 头甲耐久 护甲等级 护甲耐久 模拟次数" << std::endl;
	}

	setting.lv_bullet = std::stoi(argv[1]);
	setting.lv_helmet = std::stoi(argv[2]);
	setting.lv_armor = std::stoi(argv[4]);
	setting.dur_helmet = std::stof(argv[3]);
	setting.dur_armor = std::stof(argv[5]);
	max_times = std::stoi(argv[6]);
	verify_valid(setting.lv_bullet);
	verify_valid(setting.lv_helmet);
	verify_valid(setting.lv_armor);
	for (int i = 0; i < vg.size();i++) {
		std::cout << std::right << std::setw(2) << i << ". " << std::left << std::setw(8) << vg[i].name << "\t";
		if ((i + 1) % 5 == 0)
			std::cout << std::endl;
	}
	std::cout << std::endl;
	std::cout << "switch a gun" << std::endl;
	std::string paras;
	if (!(std::cin >> paras)) {
		std::cout << "输入流错误！" << std::endl;
		return 1;
	}

	int choice = -1;

	try {
		choice = std::stoi(paras);
		if (choice < 0 || choice >= static_cast<int>(vg.size())) {
			throw std::out_of_range("选项编号超出范围");
		}
	}
	catch (const std::invalid_argument&) {
		std::cout << "输入非数字，将模拟全部枪支并按TTK排序：\n" << std::endl;
	}

	if (choice != -1) {
		simulate(vg[choice]);
		vg[choice].show_detail();
	}
	else {
		for (auto& gun : vg) {
			simulate(gun);
		}

		std::sort(vg.begin(), vg.end(), [](const Gun& a, const Gun& b) {
			auto calculate_ttk = [](const Gun& g) {
				return (static_cast<float>(g.count) / max_times - 0.0f) * 60.0f / g.rate_of_fire;
				};
			return calculate_ttk(a) < calculate_ttk(b);
			});

		for (auto& gun : vg) {
			gun.show_ttk();
		}
	}
}
