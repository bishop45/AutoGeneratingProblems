// AutoGeneratingProblems.cpp : 将棋エンジンと通信

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>
#include <windows.h>
#include <stdio.h>
#include <filesystem>
#include <boost/algorithm/string.hpp>


using namespace std;

static const string KIFU_DIR = "kifu_sfen"; //sfen棋譜フォルダ
static const string LIMIT_TIME = "20000"; //検討秒数
static const int ANSWER_LINES = 5; //取得したい行数
static const int BEGIN = 30; //検討開始手数
static const int END = 40; //検討終了手数

struct ProcessExecute
{
	ProcessExecute() { init(); }

	//プロセスの終了処理
	virtual ~ProcessExecute() 
	{
		if (pi.hProcess) 
		{
			if (::WaitForSingleObject(pi.hProcess, 1000) != WAIT_OBJECT_0) 
			{
				::TerminateProcess(pi.hProcess, 0);
			}
			::CloseHandle(pi.hProcess);
			pi.hProcess = nullptr;
		}
	}


	// 子プロセスの実行
	void run(string app_path_)
	{
		wstring app_path = to_wstring(app_path_);
		ZeroMemory(&pi, sizeof(pi));
		ZeroMemory(&si, sizeof(si));

		si.cb = sizeof(si); //構造体サイズ
		si.hStdInput = child_std_in_read; //標準入力
		si.hStdOutput = child_std_out_write; //標準出力
		si.dwFlags |= STARTF_USESTDHANDLES; //フラグ

		// Create the child process
		success = ::CreateProcess(app_path.c_str(), // ApplicationName
			NULL, // CmdLine
			NULL, // security attributes
			NULL, // primary thread security attributes
			TRUE, // handles are inherited
			0,    // creation flags
			NULL, // use parent's environment
			NULL, // use parent's current directory
				  // ここにカレントディレクトリを指定する。
				  // engine/xxx.exe を起動するなら engine/ を指定するほうがいいような気は少しする。

			&si,  // STARTUPINFO pointer
			&pi   // receives PROCESS_INFOMATION
			);

		if (!success)
			cout << "CreateProcessに失敗" << endl;
	}

	bool success; //真・偽を格納する

	static const int BUF_SIZE = 4096;
	
	string read()
	{	
	
		auto result = read_next();
		if (!result.empty())
			return result;
	
		// ReadFileは同期的に使いたいが、しかしデータがないときにブロックされるのは困るので
		// pipeにデータがあるのかどうかを調べてからReadFile()する。

		DWORD dwRead, dwReadTotal, dwLeft;
		CHAR chBuf[BUF_SIZE];

		// bufferサイズは1文字少なく申告して終端に'\0'を付与してstring化する。

		BOOL success = ::PeekNamedPipe(
			child_std_out_read, // [in]  handle of named pipe
			chBuf,              // [out] buffer     
			BUF_SIZE - 1,         // [in]  buffer size
			&dwRead,            // [out] bytes read
			&dwReadTotal,       // [out] total bytes avail
			&dwLeft             // [out] bytes left this message
			);

		if (success && dwReadTotal > 0)
		{
			//cout << "読み込み成功" << endl;
			success = ::ReadFile(child_std_out_read, chBuf, BUF_SIZE - 1, &dwRead, NULL);

			if (success && dwRead != 0)
			{
				chBuf[dwRead] = '\0'; // 終端マークを書いて文字列化する。
				read_buffer += string(chBuf);
			}
		}
		return read_next();
	}
	
	bool write(string str)
	{
		str += "\r\n"; // 改行コードの付与
		DWORD dwWritten;
		BOOL success = ::WriteFile(child_std_in_write, str.c_str(), DWORD(str.length()), &dwWritten, NULL);
		return success;
	}
protected:
	void init() {
		// pipeの作成
		SECURITY_ATTRIBUTES saAttr;

		saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
		saAttr.bInheritHandle = TRUE;
		saAttr.lpSecurityDescriptor = NULL;

		if (!::CreatePipe(&child_std_out_read, &child_std_out_write, &saAttr, 0))
			cout << "create pipe error std out" << endl;
		if (!::SetHandleInformation(child_std_out_read, HANDLE_FLAG_INHERIT, 0))
			cout << "error SetHandleInformation : std out" << endl;
		if (!::CreatePipe(&child_std_in_read, &child_std_in_write, &saAttr, 0))
			cout << "create pipe error std in" << endl;
		if (!::SetHandleInformation(child_std_in_write, HANDLE_FLAG_INHERIT, 0))
			cout << "error SetHandleInformation : std out" << endl;
	}

	
	string read_next()
	{
		// read_bufferから改行までを切り出す
		auto it = read_buffer.find("\n");
		if (it == string::npos)
			return string();
		// 切り出したいのは"\n"の手前まで(改行コード不要)、このあと"\n"は捨てたいので
		// it+1から最後までが次回まわし。
		auto result = read_buffer.substr(0, it);
		read_buffer = read_buffer.substr(it + 1, read_buffer.size() - it);
		// "\r\n"かも知れないので"\r"も除去。
		if (result.size() && result[result.size() - 1] == '\r')
			result = result.substr(0, result.size() - 1);
		if (result.find("Error") != string::npos)
		{
			// 何らかエラーが起きたので表示させておく。
			cout << "Error : " << result << endl;
		}

		return result;
	}
	// wstring変換
	wstring to_wstring(const string& src)
	{
		size_t ret;
		wchar_t *wcs = new wchar_t[src.length() + 1];
		::mbstowcs_s(&ret, wcs, src.length() + 1, src.c_str(), _TRUNCATE);
		wstring result = wcs;
		delete[] wcs;
		return result;
	}

	PROCESS_INFORMATION pi;
	STARTUPINFO si;

	HANDLE child_std_out_read;
	HANDLE child_std_out_write;
	HANDLE child_std_in_read;
	HANDLE child_std_in_write;

	// 受信バッファ
	string read_buffer;
};

struct EngineState
{
	void run(string path)
	{
		pe.run(path);
		state = START_UP;
		engine_exe_name_ = path;
	}

	// エンジンに対する終了処理
	~EngineState()
	{
		// 思考エンジンにquitコマンドを送り終了する
		// プロセスの終了は~ProcessNegotiator()で待機し、
		// 終了しなかった場合はTerminateProcess()で強制終了する。
		pe.write("quit");
	}

	enum State {
		START_UP, WAIT_USI_OK, IS_READY, WAIT_READY_OK, GAME_START,
	};

	void on_idle()
	{
		switch (state)
		{
		case START_UP:
			pe.write("usi");
			state = WAIT_USI_OK;
			Sleep(2000);
			//string ll = pe.read();
			//cout << ll << endl;
			break;

		case WAIT_USI_OK:
		{
			string line = pe.read();
			//cout << line << endl;
			if (line == "usiok")
				state = IS_READY;
			else if (line.substr(0, min(line.size(), 8)) == "id name ")
				engine_name_ = line.substr(8, line.size() - 8);
			break;
		}
		case IS_READY:
			// エンジンの初期化コマンドを送ってやる
			for (auto line : engine_config)
				pe.write(line);

			pe.write("isready");
			state = WAIT_READY_OK;
			Sleep(2000);
			break;
		case WAIT_READY_OK:
			if (pe.read() == "readyok")
			{
				cout << "readyok" << endl;
				pe.write("usinewgame");
				state = GAME_START;
			}
			break;
		case GAME_START:
			break;
		}
	}

	string think(string pos)
	{
		pe.write(pos);
		string go = "go byoyomi " + LIMIT_TIME;
		pe.write(go);
		//cout << "now go" << endl;
		vector <string> out;
		string out_line;
		string answer;
		while (true)
		{
			out_line = pe.read();
			if (!out_line.empty())
			{
				out.push_back(out_line);
			}
			if (out_line.find("bestmove") != string::npos) 
			{
				//cout << l_size << endl;
				for (int i = ANSWER_LINES; i > 0; i--)
				{
					answer += out[int(out.size()) - i] + "\n";
				}
				cout << answer << endl;
				break;
			}
		}
		return answer;

	}

	// 対局の準備が出来たのか？
	bool is_game_started() const { return state == GAME_START; }
	// エンジンの初期化時に渡したいメッセージ
	void set_engine_config(vector<string>& lines) { engine_config = lines; }

	ProcessExecute pe;
	
protected:
	// 内部状態
	State state;
	// エンジン起動時に送信すべきコマンド
	vector<string> engine_config;

	// usiコマンドに対して思考エンジンが"is name ..."で返してきたengine名
	string engine_name_;

	// 実行したエンジンのバイナリ名
	string engine_exe_name_;

};

namespace {
	// 思考エンジンの実行ファイル名
	string engine_name;

	// usiコマンドに応答として返ってきたエンジン名
	string usi_engine_name;

	// 思考エンジンの設定
	vector<string> engine_config_line;

	//棋譜ファイルリスト
	vector<tr2::sys::path> file_list;

	//棋譜
	vector<string> kifu_list;

	//読み筋
	string bestmoves;
}

void load_config()
{
	fstream f;
	f.open("engine_config.txt");
	getline(f, engine_name);
	auto& lines = engine_config_line;
	lines.clear();
	string line;
	while (!f.eof())
	{
		getline(f, line);
		if (!line.empty())
			lines.push_back(line);
	}
	f.close();
}

int check_capture(string str1, string str2) 
{
	for (size_t c = str1.find_first_of("+"); c != string::npos; c = c = str1.find_first_of("+")) 
	{
		str1.erase(c, 1);
	}
	for (size_t c = str2.find_first_of("+"); c != string::npos; c = c = str2.find_first_of("+")) 
	{
		str2.erase(c, 1);
	}
	string str1_tmp = str1.substr(str1.size() - 2, 2);
	string str2_tmp = str2.substr(str2.size() - 2, 2);
	return (str1_tmp == str2_tmp);
}

vector<string> load_kifu()
{
	
	tr2::sys::path k_dir(KIFU_DIR);
	// k_dir以下のファイル名を取得する
	// 再帰的にファイル名を取得する場合は、std::tr2::sys::recursive_directory_iteratorを使う
	for (tr2::sys::directory_iterator it(k_dir), end; it != end; ++it) 
	{
		file_list.push_back(it->path());
	}

	/*
	// 取得したファイル名をすべて表示する
	for (auto &path : file_list) {
		std::cout << path << std::endl;
	}*/

	vector<string> l_kifu;
	for(auto &path : file_list) {
		fstream f_kifu;
		f_kifu.open(path);
		string temp;
		getline(f_kifu, temp);
		l_kifu.push_back(temp);
		/*
		while (getline(f_kifu, temp, ' '))
		{
			l_kifu.push_back(temp);
		}*/
	}
	return 	l_kifu;
	/*
	for (unsigned i = 0; i < l_kifu.size(); i++)
	{
		std::cout << l_kifu[i] << std::endl;
	}*/

}
int main()
{
	//設定のロード
	load_config();
	
	//棋譜のロード
	kifu_list = load_kifu();
	if (kifu_list.empty())
		return 0;

	//保存用ファイル
	ofstream ofs("problems.txt");

	bool game_started = false;

	EngineState es;
	es.run(engine_name);
	// プロセスの生成に失敗しているなら終了。
	if (!es.pe.success)
		return 0;

	es.set_engine_config(engine_config_line);
	for (int  i= 0; i < (int)(kifu_list.size()); i++)
	{
		for (int j = BEGIN+1; j < END+1;)
		{
			es.on_idle();
			if (!game_started && es.is_game_started())
			{
				cout << (i+1) << "/" << (int)(kifu_list.size()) << " Kifu_file, " << (j-1) << " moves"<< endl;
				vector <string> tmp;
				boost::algorithm::split(tmp, kifu_list[i], boost::algorithm:: is_space());
				//cout << tmp[j - 1] << " " << tmp[j] << endl;
				if (!check_capture(tmp[j - 1], tmp[j]))
				{
					string position = "position";
					for (int k = 0; k < j-1; k++)
					{
						position += " " + tmp[k];
					}
					ofs << position << endl;
					bestmoves = es.think(position);
					ofs << bestmoves << endl;
				}
				j++;
			}
		}
	}
	return 0;
}

