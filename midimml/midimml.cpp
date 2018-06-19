//File Created at 2018/6/15 13:45:41

/*
需要处理的消息：
主音色变换OK，CC0音色变换（音色+128，要变量）OK，速度变换OK，音量变化OK，CC#10平衡变化OK，音符开/关（要变量）OK
暂不处理的消息：
*滑音，*发出量控制（Expression），*震音（Modulation）

MML消息：
音色变换OK，音量OK，八度变换（要变量）OK，*默认音符长度定义（有默认值为4故可略），速度变换OK，
音符OK，休止符OK，循环点（CC#111位置）OK，*量化修改，*LFO设置，*SSG波形设置
*/


#define APP_NAME	"MIDI to MML Converter Tool"
#define STRING_NOT_SET	"(NOT SET)"

#ifdef _DEBUG
#pragma comment(lib,"..\\midifile\\visual-studio\\DebugLib\\midifile.lib")
#else
#pragma comment(lib,"..\\midifile\\visual-studio\\ReleaseLib\\midifile.lib")
#endif

#include "..\midifile\include\MidiFile.h"

#include<fstream>
#include<sstream>
#include<iostream>
#include<ctime>
#include<algorithm>
#include<Windows.h>

#define NOTE_OFF_NUMBER 255
#define CHANNEL_COUNT	10
#define ZENLEN_DEFAULT	96
#define NOTE_DIVNUM_INFINITY	256.0f//表示事件长度为0
namespace MMLNotes
{
	const char *mml_note_name[12] = { "c","c+","d","d+","e","f","f+","g","g+","a","a+","b" };
	const char *mml_rest_name = "r";
	const char *GetMMLNoteName(int n)
	{
		if (n == NOTE_OFF_NUMBER)
			return mml_rest_name;
		else
			return mml_note_name[n%std::size(mml_note_name)];
	}
}

//note_divnum:几分音符
std::string GetNoteQuantitized(int note, float note_divnum, int deflen,int zenlen)
{
	std::stringstream ss;
	std::vector<int>cdivnum;//zenlen的整除数（可以使用几分音符）
	for (int i = 1; i <= zenlen / 2; i++)
		if (zenlen%i == 0)
			cdivnum.push_back(i);
	float note_ticks = zenlen / note_divnum;
	int ticksum = 0;
	while (ticksum<note_ticks)
	{
		while (ticksum + zenlen / cdivnum.front() > note_ticks)
			cdivnum.erase(cdivnum.begin());
		if (cdivnum.empty())
		{
			std::cout << "遇到解析度超出范围的音符\""<<MMLNotes::GetMMLNoteName(note)<<"\", 分度："<<note_divnum<<"分音符，时钟长度："<<note_ticks<<"。\n";
			break;
		}
		if (ss.str().length() > 0)
			ss << "&";
		ss << MMLNotes::GetMMLNoteName(note);
		if (cdivnum.front() != deflen)
			ss << cdivnum.front();
		ticksum += zenlen / cdivnum.front();
	}
	return ss.str();
}

class FMChannelBase
{
private:
	std::stringstream ssChannel;

	uint8_t cc_sub;//奇数时+128，偶数时不加
	uint8_t last_note_num;//0~255,255=Off
	uint8_t last_note_volume;
	int last_note_tick;
	int oct_offset_from_midi;//MIDI到MML的八度偏移
	int deflen;
	int zenlen;
	int last_midi_octave;
public:
	FMChannelBase():zenlen(ZENLEN_DEFAULT)
	{
		SetOctaveOffsetToMIDI(0);
	}
	std::string GetStrChannel()
	{
		return ssChannel.str();
	}
	void Clear()
	{
		ssChannel.str("");
	}
	std::stringstream &GetChannelStream()
	{
		return ssChannel;
	}
	FMChannelBase &AddString(const char *data)
	{
		ssChannel << data;
		return *this;
	}
	FMChannelBase &AddInt(int data)
	{
		ssChannel << data;
		return *this;
	}
	virtual void AddChannelInit()
	{
		cc_sub = 0;
		last_note_num = NOTE_OFF_NUMBER;
		last_note_tick = 0;
		AddSetAbsoluteOctaveFromMIDI(5);
		AddSetDefaultNoteLength(4);
		AddSetVolume(127);
	}
	//设置MIDI到MML的八度偏移
	void SetOctaveOffsetToMIDI(int offset)
	{
		oct_offset_from_midi = offset;
	}
	//PMDB2:1~5
	void AddSetAbsoluteOctaveFromMIDI(int o)
	{
		last_midi_octave = o;
		AddString("o").AddInt(last_midi_octave + oct_offset_from_midi);
	}
	//设置默认音符长度为l分音符
	void AddSetDefaultNoteLength(int l)
	{
		deflen = l;
		AddString("l").AddInt(deflen);
	}
	//设置全音符时钟长度
	void AddSetZenlen(int z)
	{
		zenlen = z;
		AddString("C").AddInt(zenlen);
	}
	//0~127
	int AddSetProgram(int program)
	{
		if (cc_sub % 2 == 1)
			program += 128;
		AddString("@").AddInt(program);
		return program;
	}
	//0~127
	void SetSubProgram(uint8_t sub)
	{
		cc_sub = sub;
	}
	//0~127（任何通道，MIDI与参数的数值是等价的），用力度表示音量
	virtual void AddSetVolume(uint8_t v)
	{
		AddString("V").AddInt(v);
	}
	//36~511
	void AddSetTempo(int tempo_of_quart_note)
	{
		AddString("t").AddInt(tempo_of_quart_note / 2);
	}
	//1=R,2=L,3=Center
	void AddSetPan(int pan)
	{
		AddString("p").AddInt(pan);
	}
	//true:向右偏移八度，false:向左偏移八度
	void AddOctaveShift(bool toRight)
	{
		AddString(toRight ? ">" : "<");
	}
	void AddSetNote(int tick, uint8_t note, uint8_t vol, int tpq)
	{
		//需要处理的事情有：
		//Tick差对应的音符长，八度，音量变化，写入音符
		float note_divnum = NOTE_DIVNUM_INFINITY;//有效取值范围为1~255，INFINITY表示该事件长度为0,TODO:如果是好几节的长度怎么办？
		if (tick > last_note_tick)
			note_divnum = 4.0f*tpq / (tick - last_note_tick);//上一个音符是几分音符
		if (note_divnum != NOTE_DIVNUM_INFINITY)
			AddString(GetNoteQuantitized(last_note_num, note_divnum, deflen, zenlen).c_str());

		if (note != NOTE_OFF_NUMBER)
		{
			int delta_octave = note / 12 - last_midi_octave;
			last_midi_octave += delta_octave;
			if (delta_octave > 0)
			{
				do
				{
					AddOctaveShift(true);
				} while (--delta_octave);
			}
			else if (delta_octave < 0)
			{
				do
				{
					AddOctaveShift(false);
				} while (++delta_octave);
			}
			if (vol != last_note_volume)
			{
				AddSetVolume(vol);
				last_note_volume = vol;
			}
		}
		last_note_tick = tick;
		last_note_num = note;
	}
	void AddLoopPoint()
	{
		AddString("L");
	}
};

class FMChannel :public FMChannelBase
{
public:
	//FM:0~127
	void AddSetVolume(uint8_t v)override
	{
		return FMChannelBase::AddSetVolume(v);
	}
	void AddChannelInit()override
	{
		FMChannelBase::AddChannelInit();
		AddSetProgram(0);
	}
};

class SSGChannel :public FMChannelBase
{
public:
	//SSG:0~15
	void AddSetVolume(uint8_t v)override
	{
		return FMChannelBase::AddSetVolume(v / 8);
	}
};

class RhythmDef
{
private:
	std::vector<std::string> vRhythms;
	std::stringstream vCurRhythm;
public:
	void AddString(const char *data)
	{
		vCurRhythm << data;
	}
	void CommitRhythmDef()
	{
		vRhythms.push_back(vCurRhythm.str());
		vCurRhythm.clear();
	}
	bool IsCurrentRhythmDefined()
	{
		for (auto&v : vRhythms)
			if (v==vCurRhythm.str())
				return true;
		return false;
	}
};

//PMD专用格式MML文件
class MMLData
{
private:
	std::stringstream ssMML;
	FMChannel fmChannelsFMPart[6];//ABC DEF
	SSGChannel fmChannelsSSGPart[4];//GHI K
	FMChannelBase *pfmChannels[CHANNEL_COUNT];

	//R定义采用按小节定义的方式，从后往前查重，这样可以减少一些重复。

	const char *channelNames[10] = { "A","B","C","D","E","F","G","H","I","K" };
	int beatsPerBar;
public:
	MMLData()
	{
		for (int i = 0; i < CHANNEL_COUNT; i++)
			if (i < 6)
				pfmChannels[i] = &fmChannelsFMPart[i];
			else
				pfmChannels[i] = &fmChannelsSSGPart[i - 6];
	}
	void Init()
	{
		ssMML.clear();
		for (auto&v : pfmChannels)
			v->Clear();
		beatsPerBar = 4;//建议每4节提交一次
		InitAllChannels();
		CommitAllChannelData();
	}
	void AddMetaString(const char *meta_name,const char *str)
	{
		ssMML << "#" << meta_name;
		for (int c = max(1, 2 - ((int)strlen(meta_name) + 1) / 8) - 1; c >= 0; c--)
			ssMML << "\t";
		ssMML << str << std::endl;
	}
	void AddMetaValue(const char *meta_name, int val)
	{
		ssMML << "#" << meta_name;
		for (int c = max(1, 2 - ((int)strlen(meta_name) + 1) / 8) - 1; c >= 0; c--)
			ssMML << "\t";
		ssMML << val << std::endl;
	}
	struct MetaNames
	{
		const char *meta_title = "Title",//（文件名）
			*meta_composer = "Composer",//（用户账户名字）
			*meta_arranger = "Arranger",//APP_NAME
			*meta_memo = "Memo",//Converted at （时间）
			*meta_detune = "Detune",//Extended
			*meta_dt2flag = "DT2Flag",//On
			*meta_filename = "Filename",//.M2
			*meta_transpose = "Transpose",//默认0
			*meta_tempo = "Tempo",//默认60（120）
			*meta_option = "Option",// /v/c
			*meta_octave = "Octave",//Normal
			*meta_fffile = "FFFile",//音色定义文件
			*meta_zenlen = "Zenlen";//默认全音符时钟长度96

		//八度需要在通道里设置
	}metaNames;
	void CommitChannelData(int ch)
	{
		if (pfmChannels[ch]->GetChannelStream().str().length() > 0)
		{
			ssMML << channelNames[ch] << "\t" << pfmChannels[ch]->GetStrChannel() << std::endl;
			pfmChannels[ch]->Clear();
		}
	}
	void CommitAllChannelData()
	{
		for (size_t i = 0; i < std::size(pfmChannels); i++)
			CommitChannelData(i);
	}
	void AddNewLine()
	{
		ssMML << std::endl;
	}
	int SaveToFile(const wchar_t *file)
	{
		std::ofstream fo(file);
		if (!fo)return -1;
		CommitAllChannelData();
		fo << ssMML.str();
		return 0;
	}
	void AddComment(const char *str)
	{
		ssMML << ";" << str << std::endl;
	}
	//如果通道号大于9则返回第三通道
	FMChannelBase &GetChannel(unsigned ch)
	{
		if (ch >= CHANNEL_COUNT)
			ch = 2;
		return *pfmChannels[ch];
	}
	void SetBeatsPerBar(int bpb)
	{
		beatsPerBar = bpb;
		CommitAllChannelData();
		AddNewLine();
	}
	void InitAllChannels()
	{
		for (auto&c : pfmChannels)
			c->AddChannelInit();
	}
};

struct ProgramChangeCollector
{
	std::vector<int>programs;
	bool AddProgram(int p)
	{
		for (auto&v : programs)
			if (p == v)
				return false;
		programs.push_back(p);
		return true;
	}
	void SortAscending()
	{
		std::sort(programs.begin(), programs.end());
	}
};

std::string ToStringA(const wchar_t *str)
{
	char stra[256];
	size_t cvt = 0;
	if (wcstombs_s(&cvt, stra, str, std::size(stra) - 1) != 0)
		return std::string(STRING_NOT_SET);
	return std::string(stra);
}

std::string GetFileNameFromPath(const wchar_t *path)
{
	char name[_MAX_PATH];
	strcpy_s(name, ToStringA(path).c_str());
	int cdot, cslash;
	char *pos = strrchr(name, '.');
	if (pos == NULL)
		cdot = strlen(name);
	else
		cdot = pos - name;
	pos = strrchr(name, '\\');
	if (pos == NULL)
		pos = strrchr(name, '/');
	if (pos == NULL)
		cslash = 0;
	else
		cslash = pos - name + 1;
	strncpy_s(name, name + cslash, cdot - cslash);
	return std::string(name);
}


int ConvertToMML(const wchar_t *midi_file, const wchar_t *mml_file,int oct_offset,const wchar_t *ff_file)
{
	MMLData mml;
	mml.AddMetaString(mml.metaNames.meta_title, GetFileNameFromPath(midi_file).c_str());
	
	char strbuf[256];
	DWORD cstrbuf = std::size(strbuf);
	if (GetUserNameA(strbuf, &cstrbuf) == 0)
		strcpy_s(strbuf, STRING_NOT_SET);
	mml.AddMetaString(mml.metaNames.meta_composer, strbuf);
	mml.AddMetaString(mml.metaNames.meta_arranger, APP_NAME);
	time_t time1 = time(NULL);
	tm timedata1;
	localtime_s(&timedata1, &time1);
	strftime(strbuf, std::size(strbuf), "Converted at %x %X", &timedata1);
	mml.AddMetaString(mml.metaNames.meta_memo, strbuf);
	mml.AddMetaString(mml.metaNames.meta_detune, "Extended");
	mml.AddMetaString(mml.metaNames.meta_dt2flag, "On");
	mml.AddMetaString(mml.metaNames.meta_filename, ".M2");
	mml.AddMetaValue(mml.metaNames.meta_transpose, 0);
	mml.AddMetaValue(mml.metaNames.meta_tempo, 60);
	mml.AddMetaString(mml.metaNames.meta_option, "/v/c");
	mml.AddMetaString(mml.metaNames.meta_octave, "Normal");
	mml.AddMetaValue(mml.metaNames.meta_zenlen, ZENLEN_DEFAULT);
	if (wcslen(ff_file) > 0)
		mml.AddMetaString(mml.metaNames.meta_fffile, ToStringA(ff_file).c_str());
	mml.AddNewLine();

	mml.AddComment("==============");
	mml.AddComment("Init");
	mml.AddComment("==============");
	for (int i = 0; i < CHANNEL_COUNT; i++)
		mml.GetChannel(i).SetOctaveOffsetToMIDI(oct_offset);
	mml.Init();
	mml.AddNewLine();

	mml.AddComment("==============");
	mml.AddComment("Main");
	mml.AddComment("==============");
	smf::MidiFile mf(ToStringA(midi_file).c_str());
	mf.joinTracks();
	smf::MidiEventList &mt = mf[0];
	ProgramChangeCollector pcc;//音色收集器
	for (int i = 0; i < mt.getEventCount(); i++)
	{
		if (mt[i].isMeta())
		{
			if (mt[i].isTempo())
			{
				for (int c = 0; c < CHANNEL_COUNT; c++)
					mml.GetChannel(c).AddSetTempo((int)mt[i].getTempoBPM());
			}
			else if (mt[i].isTimeSignature())
			{
				//https://github.com/lxfly2000/libMidiPlayer/blob/lib_static/MidiPlayer.cpp#L266
				mml.SetBeatsPerBar(mt[i].data()[3]);
			}
		}
		else
		{
			//https://github.com/lxfly2000/libMidiPlayer/blob/lib_static/MidiPlayer.cpp#L220
			switch (mt[i].getP0() & 0xF0)
			{
			case 0x90://音符开
				if (mt[i].getP2() == 0)//如果音符开的力度为0也视为音符关
					mml.GetChannel(mt[i].getChannel()).AddSetNote(mt[i].tick, NOTE_OFF_NUMBER, mt[i].getP2(), mf.getTPQ());
				else
					mml.GetChannel(mt[i].getChannel()).AddSetNote(mt[i].tick, mt[i].getP1(), mt[i].getP2(), mf.getTPQ());
				break;
			case 0x80://音符关
				mml.GetChannel(mt[i].getChannel()).AddSetNote(mt[i].tick, NOTE_OFF_NUMBER, mt[i].getP2(), mf.getTPQ());
				break;
			case 0xC0://音色变换
				pcc.AddProgram(mml.GetChannel(mt[i].getChannel()).AddSetProgram(mt[i].getP1()));
				break;
			case 0xB0://CC控制器
				switch (mt[i].getP1())
				{
				case 0x00://子音色变换（CC0）
					mml.GetChannel(mt[i].getChannel()).SetSubProgram(mt[i].getP2());
					break;
				case 0x0A://平衡变化
					//http://blog.sina.com.cn/s/blog_d244318b0102xipc.html
					switch (mt[i].getP2() / 32)
					{
					case 0:
						mml.GetChannel(mt[i].getChannel()).AddSetPan(2);
						break;
					case 3:
						mml.GetChannel(mt[i].getChannel()).AddSetPan(1);
						break;
					default:
						mml.GetChannel(mt[i].getChannel()).AddSetPan(3);
						break;
					}
					break;
				case 111://RPG Maker循环点
					for (int i = 0; i < CHANNEL_COUNT; i++)
						mml.GetChannel(i).AddLoopPoint();
					break;
				case 0x32://子音色变换（CC32）
				case 0x01://Modulation
				case 0x11://Expression
					//这些消息暂不处理
					break;
				}
				break;
			}
			//TODO:节奏通道怎么处理？
		}
	}
	mml.CommitAllChannelData();
	mml.AddNewLine();
	mml.AddComment("==============");
	mml.AddComment("Using Programs");
	mml.AddComment("==============");
	pcc.SortAscending();
	std::stringstream sspcc;
	for (auto&p : pcc.programs)
	{
		if (sspcc.str().length() > 0)
			sspcc << " ";
		sspcc << "@" << p;
	}
	mml.AddComment(sspcc.str().c_str());

	return mml.SaveToFile(mml_file);
}


int wmain(int argc, wchar_t *argv[])
{
	wchar_t midi_file[_MAX_PATH] = L"", mml_file[_MAX_PATH] = L"", ff_file[_MAX_PATH] = L"";
	int oct_offset = 0;
	std::locale::global(std::locale(""));
	switch(argc)
	{
	case 5:
		wcscpy_s(ff_file, argv[4]);
	case 4:
		oct_offset = _wtoi(argv[3]);
	case 3:
		wcscpy_s(mml_file, argv[2]);
	case 2:
		wcscpy_s(midi_file, argv[1]);
		if (wcslen(mml_file) == 0)
			swprintf_s(mml_file, L"%s.mml", midi_file);
		break;
	default:
		_putws(L"PMD专用MIDI到MML转换程序 by lxfly2000\n"
			"项目网站：https://github.com/lxfly2000/midimml\n\n"
			"命令行：midimml <MIDI文件> [MML文件] [整体八度调整] [音色定义文件]");
#ifdef _DEBUG
		_getws_s(midi_file);
		wcscpy_s(mml_file, L"o.mml");
		break;
#endif
		return 1;
	}
	return ConvertToMML(midi_file, mml_file, oct_offset, ff_file);
}