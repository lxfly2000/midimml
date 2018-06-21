#ifdef _DEBUG
#pragma comment(lib,"..\\DebugLib\\midifile.lib")
#else
#pragma comment(lib,"..\\ReleaseLib\\midifile.lib")
#endif
#include"..\midifile\include\MidiFile.h"

#define MIDI_CHANNEL_MAX	16
#define MIDI_CHANNEL_DRUM	10
#define NOTE_OFF_NUMBER	-1

constexpr int midiDrumChannelIndex = MIDI_CHANNEL_DRUM - 1;

class ExchannelMgr
{
private:
	int channelNote[MIDI_CHANNEL_MAX];
public:
	ExchannelMgr()
	{
		for (int i = 0; i < (int)std::size(channelNote); i++)
			SetNoteOff(i);
	}
	void SetNoteOn(int ch, int note)
	{
		channelNote[ch] = note;
	}
	void SetNoteOff(int ch)
	{
		return SetNoteOn(ch, NOTE_OFF_NUMBER);
	}
	bool IsChannelAvailable(int ch)
	{
		return channelNote[ch] == NOTE_OFF_NUMBER;
	}
	int GetAvailableChannel(int fromCh)
	{
		int lastCh = fromCh;
		for (int i = fromCh; !(i == fromCh && lastCh != fromCh);)
		{
			if (channelNote[i] == NOTE_OFF_NUMBER)
				return i;
			lastCh = i;
			i = (i + 1) % MIDI_CHANNEL_MAX;
			if (i == midiDrumChannelIndex)
				i = (i + 1) % MIDI_CHANNEL_MAX;
		}
		return fromCh;
	}
	int GetPoly()
	{
		int p = 0;
		for (auto&n : channelNote)
			if (n != NOTE_OFF_NUMBER)
				p++;
		return p;
	}
};

int Exchannel(const char *filein, const char *fileout)
{
	smf::MidiFile mf(filein);
	//https://midifile.sapp.org
	if (!mf.status())
		return -1;
	mf.joinTracks();
	smf::MidiEventList &mt = mf[0];
	ExchannelMgr cm;
	int note_on_change = 0, note_off_change = 0;
	for (int i = 0; i < mt.getEventCount(); i++)
	{
		if (!mt[i].isMeta()&&mt[i].getChannel()!=midiDrumChannelIndex)
		{
			switch (mt[i].getP0()&0xF0)
			{
			case 0x90:
				if (mt[i].getP2() == 0)//如果音符开的力度为0也视为音符关
				{
					cm.SetNoteOff(mt[i].getChannel());
				}
				else
				{
					if (cm.IsChannelAvailable(mt[i].getChannel()))
					{
						cm.SetNoteOn(mt[i].getChannel(), mt[i].getP1());
					}
					else
					{
						int avaiChannel = cm.GetAvailableChannel(mt[i].getChannel());
						if (mt[i].getChannel() == avaiChannel)
						{
							printf("Tick:%d 处没有可用通道，放回原通道：%d\n", mt[i].tick, avaiChannel);
						}
						else
						{
							for (int j = i; j < mt.getEventCount(); j++)
							{
								if (mt[j].getP1() == mt[i].getP1() && mt[j].getChannel() == mt[i].getChannel() && mt[j].isNoteOff())
								{
									mt[j].setChannel(avaiChannel);
									note_off_change++;
									break;
								}
							}
							mt[i].setChannel(avaiChannel);
							note_on_change++;
							cm.SetNoteOn(mt[i].getChannel(), mt[i].getP1());
						}
					}
				}
				break;
			case 0x80:
				cm.SetNoteOff(mt[i].getChannel());
				break;
			}
		}
	}
	if (cm.GetPoly())
		printf("有未结束的%d个音符，这可能是原MIDI本身就有的问题。\n", cm.GetPoly());
	printf("修改了%d个音符开事件，%d个音符关事件。\n", note_on_change, note_off_change);
	if (note_on_change != note_off_change)
		puts("程序未能正确处理MIDI事件，输出的文件可能有错误。");
	mf.write(fileout);
	return 0;
}

int main(int argc, char *argv[])
{
	char filein[_MAX_PATH] = "", fileout[_MAX_PATH] = "";
	switch (argc)
	{
	case 3:
		strcpy_s(fileout, argv[2]);
	case 2:
		strcpy_s(filein, argv[1]);
		if (strlen(fileout) == 0)
		{
			int nc = strrchr(filein, '.') - filein;
			strncpy_s(fileout, filein, nc);
			strcat_s(fileout, "_ex.mid");
		}
		break;
	default:
		puts("这个程序处理MIDI文件使每个MIDI通道最多只含有一个音符，只处理音符开/关事件。\n"
			"命令行：midiexch <输入文件> [输出文件]");
#ifdef _DEBUG
		gets_s(filein);
		strcpy_s(fileout, "o.mid");
		break;
#endif
		return 1;
	}
	int r=Exchannel(filein, fileout);
	if (r == 0)
		printf("文件已保存至\"%s\".\n", fileout);
	else
		puts("出现错误。");
	return r;
}