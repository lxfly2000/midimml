//File Created at 2018/6/15 13:45:41

/*
��Ҫ�������Ϣ��
����ɫ�任OK��CC0��ɫ�任����ɫ+128��Ҫ������OK���ٶȱ任OK�������仯OK��CC#10ƽ��仯OK��������/�أ�Ҫ������OK
�ݲ��������Ϣ��
*������*���������ƣ�Expression����*������Modulation��

MML��Ϣ��
��ɫ�任OK������OK���˶ȱ任��Ҫ������OK��*Ĭ���������ȶ��壨��Ĭ��ֵΪ4�ʿ��ԣ����ٶȱ任OK��
����OK����ֹ��OK��ѭ���㣨CC#111λ�ã�OK��*�����޸ģ�*LFO���ã�*SSG��������
*/


#define APP_NAME	"MIDI to MML Converter Tool"
#define STRING_NOT_SET	"(NOT SET)"

#ifdef _DEBUG
#pragma comment(lib,"..\\DebugLib\\midifile.lib")
#else
#pragma comment(lib,"..\\ReleaseLib\\midifile.lib")
#endif

#include "..\midifile\include\MidiFile.h"

#include<fstream>
#include<sstream>
#include<iostream>
#include<ctime>
#include<algorithm>
#include<map>
#include<Windows.h>

#define NOTE_OFF_NUMBER 255
#define CHANNEL_COUNT	10
#define ZENLEN_DEFAULT	96
#define PROGRAM_DEFAULT	0
#define NOTE_DIVNUM_INFINITY	256.0f//��ʾ�¼�����Ϊ0
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

//note_divnum:��������
std::string GetNoteQuantitized(int note, float note_divnum, int deflen,int zenlen,bool isRhythmChannel)
{
	std::stringstream ss;
	std::vector<int>cdivnum;//zenlen��������������ʹ�ü���������
	for (int i = 1; i <= zenlen / 2; i++)
		if (zenlen%i == 0)
			cdivnum.push_back(i);
	float note_ticks = zenlen / note_divnum;
	int ticksum = 0;
	while (ticksum<note_ticks)
	{
		while (cdivnum.size()>0&&ticksum + zenlen / cdivnum.front() > note_ticks)
			cdivnum.erase(cdivnum.begin());
		if (cdivnum.empty())
		{
			std::cout << "���������ȳ�����Χ������\""<<MMLNotes::GetMMLNoteName(note)<<"\", �ֶȣ�"<<note_divnum<<"��������ʱ�ӳ��ȣ�"<<note_ticks<<"��\n";
			break;
		}
		if (ss.str().length() > 0)
		{
			if (isRhythmChannel)
				note = NOTE_OFF_NUMBER;
			else
				ss << "&";
		}
		ss << MMLNotes::GetMMLNoteName(note);
		if (cdivnum.front() != deflen)
			ss << cdivnum.front();
		ticksum += zenlen / cdivnum.front();
	}
	return ss.str();
}

class FMChannelBase
{
protected:
	std::stringstream ssChannel;
	std::string channel_name;
	uint8_t last_note_volume;
	int last_note_tick;
	int deflen;
	int zenlen;
private:
	uint8_t cc_sub;//����ʱ+128��ż��ʱ����
	uint8_t last_note_num;//0~255,255=Off
	int oct_offset_from_midi;//MIDI��MML�İ˶�ƫ��
	int last_midi_octave;
public:
	FMChannelBase():zenlen(ZENLEN_DEFAULT)
	{
		SetOctaveOffsetToMIDI(0);
	}
	void SetZenlenValue(int z)
	{
		zenlen = z;
	}
	void SetChannelName(const char *c)
	{
		channel_name.assign(c);
	}
	virtual std::string GetStrToCommit()
	{
		std::stringstream dataToCommit;
		dataToCommit << channel_name << "\t" << ssChannel.str() << std::endl;
		return dataToCommit.str();
	}
	virtual void Clear()
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
	//����MIDI��MML�İ˶�ƫ��
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
	//����Ĭ����������Ϊl������
	void AddSetDefaultNoteLength(int l)
	{
		deflen = l;
		AddString("l").AddInt(deflen);
	}
	//����ȫ����ʱ�ӳ���
	void AddSetZenlen(int z)
	{
		zenlen = z;
		AddString("C").AddInt(zenlen);
	}
	//0~127
	virtual int AddSetProgram(int program)
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
	//0~127���κ�ͨ����MIDI���������ֵ�ǵȼ۵ģ��������ȱ�ʾ����
	virtual void AddSetVolume(uint8_t v)
	{
		AddString("V").AddInt(v);
	}
	//36~511, ������;���٣�ֻ����һ��ͨ����ָ�������趨����ͨ�����ٶ�
	//ע�⣺��ָ�����Ե�ǰλ���ѹرյģ�Ҳ���������ģ�������Ч����˸�ָ�������Ҫ������ʵ�ʵı���λ������ƫ�ƴ���0��������λ���ϡ�
	void AddSetTempo(int tempo_of_quart_note)
	{
		AddString("t").AddInt(tempo_of_quart_note / 2);
	}
	//1=R,2=L,3=Center
	void AddSetPan(int pan)
	{
		AddString("p").AddInt(pan);
	}
	//true:����ƫ�ư˶ȣ�false:����ƫ�ư˶�
	void AddOctaveShift(bool toRight)
	{
		AddString(toRight ? ">" : "<");
	}
	virtual void AddSetNote(int tick, uint8_t note, uint8_t velocity, int tpq)
	{
		//��Ҫ����������У�
		//Tick���Ӧ�����������˶ȣ������仯��д������
		float note_divnum = NOTE_DIVNUM_INFINITY;//��Чȡֵ��ΧΪ1~255��INFINITY��ʾ���¼�����Ϊ0
		if (tick > last_note_tick)
			note_divnum = 4.0f*tpq / (tick - last_note_tick);//��һ�������Ǽ�������
		if (note_divnum != NOTE_DIVNUM_INFINITY)
			AddString(GetNoteQuantitized(last_note_num, note_divnum, deflen, zenlen,false).c_str());

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
			if (velocity != last_note_volume)
			{
				AddSetVolume(velocity);
				last_note_volume = velocity;
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
		AddSetProgram(PROGRAM_DEFAULT);
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

/*class RhythmDef
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
};*/

class KRhythmChannel :public FMChannelBase
{
private:
	int r_counter;
	std::map<int,int>midi_to_mml_rhythm;//K:MIDI��λ��V:MMLֵ
	int mem_note_num;//���ڱ�����ɫ�任֮ǰ��һ����ɫ
	int note_off_tick;
	int last_tpq;
	int last_note_num;
public:
	KRhythmChannel():r_counter(0),note_off_tick(0),last_tpq(0)
	{
		//https://github.com/lxfly2000/pmdplay
		std::pair<int, int> table[] = {
			{36,1},//	��Bass
			{37,32},//	��Rim
			{38,2},//	��Snare
			{39,64},//	��Clap
			{40,64},//	��SD2
			{41,4},//	��Low Tom
			{42,128},//	��Closed Hat
			{43,4},//	��Low Tom
			{44,128},//	��Pedal Hat
			{45,8},//	��Mid Tom
			{46,256},//	��Open Hat
			{47,8},//	��Mid Tom
			{48,16},//	��Hi Tom
			{49,512},//	��Crash Cymbal 1
			{50,16},//	��Hi Tom
			{51,1024},//	��Ride Cymbal 1
			{52,512},//	��Chinese Cymbal
			{53,1024},//	��Ride Bell
			{54,1024},//	��Tambourine
			{55,512},//	��Splash Cymbal
			{56,32},//	��Cowbell
			{57,512},//	��Crash Cymbal 2
			{58,1024},//	��Vibra-slap
			{59,1024},//	��Ride Cymbal 2
		};
		for (auto&p : table)
			midi_to_mml_rhythm.insert(p);
	}
	void AddChannelInit()override
	{
		last_note_num = 0;//�ڴ˴���ʾ@ֵ
		mem_note_num = -1;
	}
	void AddChannelInitInRDef()
	{
		FMChannelBase::AddChannelInit();//��ΪR�����ǻ�������ģ�KR��ʼ��Ӧ�����ÿ��R������
	}
	void AddSetNote(int tick, uint8_t note, uint8_t velocity, int tpq)override
	{
		//ע��˴���note�Ƿ��ڳ���任���
		if (note == NOTE_OFF_NUMBER)
		{
			note_off_tick = tick;
			return;
		}
		else if (velocity == 0)
			return;
		//https://blog.csdn.net/txh0001/article/details/6243295
		else if (midi_to_mml_rhythm.find(note) == midi_to_mml_rhythm.end())
			return;
		last_tpq = tpq;
		float note_divnum = NOTE_DIVNUM_INFINITY;//��Чȡֵ��ΧΪ1~255��INFINITY��ʾ���¼�����Ϊ0
		if (tick > last_note_tick)
			note_divnum = 4.0f*tpq / (tick - last_note_tick);//��һ�������Ǽ�������
		if (note_divnum == NOTE_DIVNUM_INFINITY)//��֮ǰ��ͬ��tick
		{
			last_note_num |= midi_to_mml_rhythm[note];
		}
		else//tick��ǰ�ƶ�
		{
			if (mem_note_num == -1)
				AddChannelInitInRDef();
			if (mem_note_num != last_note_num)
				AddSetProgram(last_note_num);
			AddString(GetNoteQuantitized(0, note_divnum, deflen, zenlen, true).c_str());
			last_note_num = midi_to_mml_rhythm[note];
			last_note_tick = tick;
		}
		//Kͨ����������ʱ���������ݶ���
	}
	//ͬ�����࣬currentTickΪ��ͬ���ĵ�ǰʱ������Ϊĳһʱ�̣����Ϊ0�򲻸Ķ�
	void FlushNote(int currentTick)
	{
		if (last_note_num == 0)
			return;
		if (currentTick)
			note_off_tick = currentTick;
		float note_divnum;
		if (note_off_tick > last_note_tick)
			note_divnum = 4.0f*last_tpq / (note_off_tick - last_note_tick);
		else
			note_divnum = static_cast<float>(deflen);

		if (mem_note_num != last_note_num)
			AddSetProgram(last_note_num);
		AddString(GetNoteQuantitized(0, note_divnum, deflen, zenlen, true).c_str());
		last_note_num = 0;
		last_note_tick = note_off_tick;
	}
	int AddSetProgram(int program)override
	{
		AddString("@").AddInt(program);
		mem_note_num = program;
		return program;
	}
	void AddSetVolume(uint8_t v)override
	{
		return FMChannelBase::AddSetVolume(v / 8);
	}
	std::string GetStrToCommit()override
	{
		std::stringstream dataToCommit;
		FlushNote(0);
		dataToCommit << "R" << r_counter << "\t" << ssChannel.str() << std::endl << channel_name << "\tR" << r_counter << std::endl;
		return dataToCommit.str();
	}
	//Kͨ����Clear������ʹR��������1
	void Clear()override
	{
		if (ssChannel.str().length() == 0)
			return;
		r_counter++;
		if (r_counter > 255)
			std::cout << "R���ඨ�������������ޡ�\n";
		FMChannelBase::Clear();
		AddChannelInit();
	}
};

//PMDר�ø�ʽMML�ļ�
class MMLData
{
private:
	std::stringstream ssMML;
	FMChannel fmChannelsFMPart[6];//ABC DEF
	SSGChannel fmChannelsSSGPart[3];//GHI
	KRhythmChannel fmChannelsKRPart;//K
	FMChannelBase *pfmChannels[CHANNEL_COUNT];

	//R������ð�С�ڶ���ķ�ʽ���Ӻ���ǰ���أ��������Լ���һЩ�ظ���

	const char *channelNames[10] = { "A","B","C","D","E","F","G","H","I","K" };
	int beatsPerBar;
public:
	MMLData()
	{
		for (int i = 0; i < CHANNEL_COUNT; i++)
		{
			if (i < 6)
				pfmChannels[i] = &fmChannelsFMPart[i];
			else if (i < 9)
				pfmChannels[i] = &fmChannelsSSGPart[i - 6];
			else
				pfmChannels[i] = &fmChannelsKRPart;
			pfmChannels[i]->SetChannelName(channelNames[i]);
		}
	}
	void Init()
	{
		ssMML.clear();
		for (auto&v : pfmChannels)
			v->Clear();
		beatsPerBar = 4;//����ÿ4���ύһ��
		InitAllChannels();
		CommitAllChannelData(0);
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
	void AddMetaZenlen(int val)
	{
		AddMetaValue(metaNames.meta_zenlen, val);
		for (auto&c : pfmChannels)
			c->SetZenlenValue(val);
	}
	struct MetaNames
	{
		const char *meta_title = "Title",//���ļ�����
			*meta_composer = "Composer",//���û��˻����֣�
			*meta_arranger = "Arranger",//APP_NAME
			*meta_memo = "Memo",//Converted at ��ʱ�䣩
			*meta_detune = "Detune",//Extended
			*meta_dt2flag = "DT2Flag",//On
			*meta_filename = "Filename",//.M2
			*meta_transpose = "Transpose",//Ĭ��0
			*meta_tempo = "Tempo",//Ĭ��60��120��
			*meta_option = "Option",// /v/c
			*meta_octave = "Octave",//Normal
			*meta_fffile = "FFFile",//��ɫ�����ļ�
			*meta_zenlen = "Zenlen";//Ĭ��ȫ����ʱ�ӳ���96

		//�˶���Ҫ��ͨ��������
	}metaNames;
	void CommitChannelData(int ch,int currentTick)
	{
		if (ch == 9)
			((KRhythmChannel*)pfmChannels[ch])->FlushNote(currentTick);
		if (pfmChannels[ch]->GetChannelStream().str().length() > 0)
		{
			ssMML << pfmChannels[ch]->GetStrToCommit();
			pfmChannels[ch]->Clear();
		}
	}
	void CommitAllChannelData(int currentTick)
	{
		for (size_t i = 0; i < std::size(pfmChannels); i++)
			CommitChannelData(i,currentTick);
	}
	void AddNewLine()
	{
		ssMML << std::endl;
	}
	int SaveToFile(const wchar_t *file)
	{
		std::ofstream fo(file);
		if (!fo)return -1;
		CommitAllChannelData(0);
		fo << ssMML.str();
		return 0;
	}
	void AddComment(const char *str)
	{
		ssMML << ";" << str << std::endl;
	}
	void AddMMLLine(const char *str)
	{
		ssMML << str << std::endl;
	}
	//���ͨ���Ŵ���9�򷵻ص���ͨ��
	FMChannelBase &GetChannel(unsigned ch)
	{
		if (ch >= CHANNEL_COUNT)
			ch = 2;
		return *pfmChannels[ch];
	}
	void SetBeatsPerBar(int bpb,int currentTick)
	{
		beatsPerBar = bpb;
		CommitAllChannelData(currentTick);
		AddNewLine();
	}
	void InitAllChannels()
	{
		for (auto&c : pfmChannels)
			c->AddChannelInit();
	}
	void AddLoop(int currentTick)
	{
		CommitAllChannelData(currentTick);
		AddNewLine();
		AddMMLLine("ABCDEFGHIK\tL\t;Delete channels with no following notes before compiling.");
		AddNewLine();
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


int ConvertToMML(const wchar_t *midi_file, const wchar_t *mml_file,int oct_offset,int _zenlen,const wchar_t *ff_file)
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
	mml.AddMetaZenlen(_zenlen);
	if (wcslen(ff_file) > 0)
		mml.AddMetaString(mml.metaNames.meta_fffile, ToStringA(ff_file).c_str());
	mml.AddNewLine();

	mml.AddComment("==============");
	mml.AddComment("Default voices, you can modify or delete them.");
	mml.AddComment("==============");
	mml.AddMMLLine("@" _CRT_STRINGIZE(PROGRAM_DEFAULT) " 4 7\n"
		"28  0 8 0 3 35 2 12 3 0 0\n"
		"26 10 7 6 2  0 1  4 3 0 0\n"
		"28  0 8 0 3 33 2 12 7 0 0\n"
		"26 10 7 6 2  0 1  4 7 0 0");
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
	if (mf.status() != true)
	{
		std::wcout << L"��ȡ\"" << midi_file << L"\"ʧ�ܡ�\n";
		return -1;
	}
	mf.joinTracks();
	smf::MidiEventList &mt = mf[0];
	ProgramChangeCollector pcc;//��ɫ�ռ���
	pcc.AddProgram(PROGRAM_DEFAULT);
	for (int i = 0; i < mt.getEventCount(); i++)
	{
		if (mt[i].isMeta())
		{
			if (mt[i].isTempo())
			{
				mml.GetChannel(0).AddSetTempo((int)mt[i].getTempoBPM());
			}
			else if (mt[i].isTimeSignature())
			{
				//https://github.com/lxfly2000/libMidiPlayer/blob/lib_static/MidiPlayer.cpp#L266
				mml.SetBeatsPerBar(mt[i].data()[3],mt[i].tick);
			}
		}
		else
		{
			//https://github.com/lxfly2000/libMidiPlayer/blob/lib_static/MidiPlayer.cpp#L220
			switch (mt[i].getP0() & 0xF0)
			{
			case 0x90://������
				if (mt[i].getP2() == 0)//���������������Ϊ0Ҳ��Ϊ������
					mml.GetChannel(mt[i].getChannel()).AddSetNote(mt[i].tick, NOTE_OFF_NUMBER, mt[i].getP2(), mf.getTPQ());
				else
					mml.GetChannel(mt[i].getChannel()).AddSetNote(mt[i].tick, mt[i].getP1(), mt[i].getP2(), mf.getTPQ());
				break;
			case 0x80://������
				mml.GetChannel(mt[i].getChannel()).AddSetNote(mt[i].tick, NOTE_OFF_NUMBER, mt[i].getP2(), mf.getTPQ());
				break;
			case 0xC0://��ɫ�任
			{
				int _pc = mml.GetChannel(mt[i].getChannel()).AddSetProgram(mt[i].getP1());
				if (mt[i].getChannel() < 6)
					pcc.AddProgram(_pc);
			}
				break;
			case 0xB0://CC������
				switch (mt[i].getP1())
				{
				case 0x00://����ɫ�任��CC0��
					mml.GetChannel(mt[i].getChannel()).SetSubProgram(mt[i].getP2());
					break;
				case 0x0A://ƽ��仯
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
				case 111://RPG Makerѭ����
					mml.AddLoop(mt[i].tick);
					break;
				case 0x32://����ɫ�任��CC32��
				case 0x01://Modulation
				case 0x11://Expression
					//��Щ��Ϣ�ݲ�����
					break;
				}
				break;
			}
		}
	}
	mml.CommitAllChannelData(0);
	mml.AddNewLine();
	mml.AddComment("==============");
	mml.AddComment("Using voices");
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

	int r=mml.SaveToFile(mml_file);
	if (r == 0)
		std::wcout << L"�ļ��ѱ�����\""<<mml_file<<"\".\n";
	else
		_putws(L"ת��ʧ�ܡ�");
	return r;
}


int wmain(int argc, wchar_t *argv[])
{
	wchar_t midi_file[_MAX_PATH] = L"", mml_file[_MAX_PATH] = L"", ff_file[_MAX_PATH] = L"";
	int oct_offset = 0, zenlen = ZENLEN_DEFAULT;
	std::locale::global(std::locale(""));
	switch(argc)
	{
	case 6:
		wcscpy_s(ff_file, argv[5]);
	case 5:
		zenlen = _wtoi(argv[4]);
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
		wprintf(L"PMDר��MIDI��MMLת������ by lxfly2000\n"
			"��Ŀ��վ��https://github.com/lxfly2000/midimml\n\n"
			"�����У�midimml <MIDI�ļ�> [MML�ļ�] [����˶ȵ���] [ȫ������] [��ɫ�����ļ�]\n"
			"�˶ȵ���Ĭ��%d��\nȫ��������Χ��1��255��Ĭ��%d\n", oct_offset, zenlen);
#ifdef _DEBUG
		_getws_s(midi_file);
		wcscpy_s(mml_file, L"o.mml");
		break;
#endif
		return 1;
	}
	return ConvertToMML(midi_file, mml_file, oct_offset, zenlen, ff_file);
}