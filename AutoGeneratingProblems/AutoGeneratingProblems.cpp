// AutoGeneratingProblems.cpp : �����G���W���ƒʐM

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

static const string KIFU_DIR = "kifu_sfen"; //sfen�����t�H���_
static const string LIMIT_TIME = "20000"; //�����b��
static const int ANSWER_LINES = 5; //�擾�������s��
static const int BEGIN = 30; //�����J�n�萔
static const int END = 40; //�����I���萔

struct ProcessExecute
{
	ProcessExecute() { init(); }

	//�v���Z�X�̏I������
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


	// �q�v���Z�X�̎��s
	void run(string app_path_)
	{
		wstring app_path = to_wstring(app_path_);
		ZeroMemory(&pi, sizeof(pi));
		ZeroMemory(&si, sizeof(si));

		si.cb = sizeof(si); //�\���̃T�C�Y
		si.hStdInput = child_std_in_read; //�W������
		si.hStdOutput = child_std_out_write; //�W���o��
		si.dwFlags |= STARTF_USESTDHANDLES; //�t���O

		// Create the child process
		success = ::CreateProcess(app_path.c_str(), // ApplicationName
			NULL, // CmdLine
			NULL, // security attributes
			NULL, // primary thread security attributes
			TRUE, // handles are inherited
			0,    // creation flags
			NULL, // use parent's environment
			NULL, // use parent's current directory
				  // �����ɃJ�����g�f�B���N�g�����w�肷��B
				  // engine/xxx.exe ���N������Ȃ� engine/ ���w�肷��ق��������悤�ȋC�͏�������B

			&si,  // STARTUPINFO pointer
			&pi   // receives PROCESS_INFOMATION
			);

		if (!success)
			cout << "CreateProcess�Ɏ��s" << endl;
	}

	bool success; //�^�E�U���i�[����

	static const int BUF_SIZE = 4096;
	
	string read()
	{	
	
		auto result = read_next();
		if (!result.empty())
			return result;
	
		// ReadFile�͓����I�Ɏg���������A�������f�[�^���Ȃ��Ƃ��Ƀu���b�N�����͍̂���̂�
		// pipe�Ƀf�[�^������̂��ǂ����𒲂ׂĂ���ReadFile()����B

		DWORD dwRead, dwReadTotal, dwLeft;
		CHAR chBuf[BUF_SIZE];

		// buffer�T�C�Y��1�������Ȃ��\�����ďI�[��'\0'��t�^����string������B

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
			//cout << "�ǂݍ��ݐ���" << endl;
			success = ::ReadFile(child_std_out_read, chBuf, BUF_SIZE - 1, &dwRead, NULL);

			if (success && dwRead != 0)
			{
				chBuf[dwRead] = '\0'; // �I�[�}�[�N�������ĕ����񉻂���B
				read_buffer += string(chBuf);
			}
		}
		return read_next();
	}
	
	bool write(string str)
	{
		str += "\r\n"; // ���s�R�[�h�̕t�^
		DWORD dwWritten;
		BOOL success = ::WriteFile(child_std_in_write, str.c_str(), DWORD(str.length()), &dwWritten, NULL);
		return success;
	}
protected:
	void init() {
		// pipe�̍쐬
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
		// read_buffer������s�܂ł�؂�o��
		auto it = read_buffer.find("\n");
		if (it == string::npos)
			return string();
		// �؂�o�������̂�"\n"�̎�O�܂�(���s�R�[�h�s�v)�A���̂���"\n"�͎̂Ă����̂�
		// it+1����Ō�܂ł�����܂킵�B
		auto result = read_buffer.substr(0, it);
		read_buffer = read_buffer.substr(it + 1, read_buffer.size() - it);
		// "\r\n"�����m��Ȃ��̂�"\r"�������B
		if (result.size() && result[result.size() - 1] == '\r')
			result = result.substr(0, result.size() - 1);
		if (result.find("Error") != string::npos)
		{
			// ���炩�G���[���N�����̂ŕ\�������Ă����B
			cout << "Error : " << result << endl;
		}

		return result;
	}
	// wstring�ϊ�
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

	// ��M�o�b�t�@
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

	// �G���W���ɑ΂���I������
	~EngineState()
	{
		// �v�l�G���W����quit�R�}���h�𑗂�I������
		// �v���Z�X�̏I����~ProcessNegotiator()�őҋ@���A
		// �I�����Ȃ������ꍇ��TerminateProcess()�ŋ����I������B
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
			// �G���W���̏������R�}���h�𑗂��Ă��
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

	// �΋ǂ̏������o�����̂��H
	bool is_game_started() const { return state == GAME_START; }
	// �G���W���̏��������ɓn���������b�Z�[�W
	void set_engine_config(vector<string>& lines) { engine_config = lines; }

	ProcessExecute pe;
	
protected:
	// �������
	State state;
	// �G���W���N�����ɑ��M���ׂ��R�}���h
	vector<string> engine_config;

	// usi�R�}���h�ɑ΂��Ďv�l�G���W����"is name ..."�ŕԂ��Ă���engine��
	string engine_name_;

	// ���s�����G���W���̃o�C�i����
	string engine_exe_name_;

};

namespace {
	// �v�l�G���W���̎��s�t�@�C����
	string engine_name;

	// usi�R�}���h�ɉ����Ƃ��ĕԂ��Ă����G���W����
	string usi_engine_name;

	// �v�l�G���W���̐ݒ�
	vector<string> engine_config_line;

	//�����t�@�C�����X�g
	vector<tr2::sys::path> file_list;

	//����
	vector<string> kifu_list;

	//�ǂ݋�
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
	// k_dir�ȉ��̃t�@�C�������擾����
	// �ċA�I�Ƀt�@�C�������擾����ꍇ�́Astd::tr2::sys::recursive_directory_iterator���g��
	for (tr2::sys::directory_iterator it(k_dir), end; it != end; ++it) 
	{
		file_list.push_back(it->path());
	}

	/*
	// �擾�����t�@�C���������ׂĕ\������
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
	//�ݒ�̃��[�h
	load_config();
	
	//�����̃��[�h
	kifu_list = load_kifu();
	if (kifu_list.empty())
		return 0;

	//�ۑ��p�t�@�C��
	ofstream ofs("problems.txt");

	bool game_started = false;

	EngineState es;
	es.run(engine_name);
	// �v���Z�X�̐����Ɏ��s���Ă���Ȃ�I���B
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

