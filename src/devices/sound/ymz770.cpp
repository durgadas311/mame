// license:BSD-3-Clause
// copyright-holders:Olivier Galibert, R. Belmont, MetalliC
/***************************************************************************

    Yamaha YMZ770C "AMMS-A" and YMZ774 "AMMS2C"

    Emulation by R. Belmont and MetalliC
    AMM decode by Olivier Galibert

-----
TODO:
- What does channel ATBL mean?
- how triggered SAC (Simple Access Codes) playback ?
 770:
- sequencer timers and triggers not implemented (seems used in Deathsmiles ending tune)
 774:
- 4 channel output
- Equalizer
- pan delayed transition (not used in games)
- sequencer off trigger (not used in games)

 known SPUs in this series:
  YMZ770B  ????     Capcom medal hardware (alien.cpp), sample format is not AMM, in other parts looks like 770C
  YMZ770C  AMMS-A   Cave CV1000
  YMZ773   AMMS2
  YMZ775   AMMS2B
  YMZ774   AMMS2C   IGS PGM2
  YMZ776   AMMS3    uses AM3 sample format (modified Ogg?)

***************************************************************************/

#include "emu.h"
#include "ymz770.h"
#include "mpeg_audio.h"

// device type definition
DEFINE_DEVICE_TYPE(YMZ770, ymz770_device, "ymz770", "Yamaha YMZ770C AMMS-A")
DEFINE_DEVICE_TYPE(YMZ774, ymz774_device, "ymz774", "Yamaha YMZ774 AMMS2C")

//-------------------------------------------------
//  ymz770_device - constructor
//-------------------------------------------------

ymz770_device::ymz770_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: ymz770_device(mconfig, YMZ770, tag, owner, clock, 16000)
{
}

ymz770_device::ymz770_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock, uint32_t sclock)
	: device_t(mconfig, type, tag, owner, clock)
	, device_sound_interface(mconfig, *this)
	, m_stream(nullptr)
	, m_sclock(sclock)
	, m_cur_reg(0)
	, m_mute(0)
	, m_doen(0)
	, m_vlma(0)
	, m_vlma1(0)
	, m_bsl(0)
	, m_cpl(0)
	, m_rom(*this, DEVICE_SELF)
{
}


//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void ymz770_device::device_start()
{
	// create the stream
	m_stream = machine().sound().stream_alloc(*this, 0, 2, m_sclock);

	for (auto & channel : m_channels)
	{
		channel.is_playing = false;
		channel.decoder = new mpeg_audio(&m_rom[0], mpeg_audio::AMM, false, 0);
	}
	for (auto & sequence : m_sequences)
		sequence.is_playing = false;
	for (auto & sqc : m_sqcs)
		sqc.is_playing = false;

	// register for save states
	save_item(NAME(m_cur_reg));
	save_item(NAME(m_mute));
	save_item(NAME(m_doen));
	save_item(NAME(m_vlma));
	save_item(NAME(m_vlma1));
	save_item(NAME(m_bsl));
	save_item(NAME(m_cpl));

	for (int ch = 0; ch < 16; ch++) // TODO array size
	{
		save_item(NAME(m_channels[ch].phrase), ch);
		save_item(NAME(m_channels[ch].pan), ch);
		save_item(NAME(m_channels[ch].pan_delay), ch);
		save_item(NAME(m_channels[ch].pan1), ch);
		save_item(NAME(m_channels[ch].pan1_delay), ch);
		save_item(NAME(m_channels[ch].volume), ch);
		save_item(NAME(m_channels[ch].volume_target), ch);
		save_item(NAME(m_channels[ch].volume_delay), ch);
		save_item(NAME(m_channels[ch].volume2), ch);
		save_item(NAME(m_channels[ch].loop), ch);
		save_item(NAME(m_channels[ch].is_playing), ch);
		save_item(NAME(m_channels[ch].last_block), ch);
		save_item(NAME(m_channels[ch].is_paused), ch);
		save_item(NAME(m_channels[ch].output_remaining), ch);
		save_item(NAME(m_channels[ch].output_ptr), ch);
		save_item(NAME(m_channels[ch].atbl), ch);
		save_item(NAME(m_channels[ch].pptr), ch);
		save_item(NAME(m_channels[ch].output_data), ch);
	}
	for (int ch = 0; ch < 8; ch++)
	{
		save_item(NAME(m_sequences[ch].delay), ch);
		save_item(NAME(m_sequences[ch].sequence), ch);
		save_item(NAME(m_sequences[ch].timer), ch);
		save_item(NAME(m_sequences[ch].stopchan), ch);
		save_item(NAME(m_sequences[ch].loop), ch);
		save_item(NAME(m_sequences[ch].bank), ch);
		save_item(NAME(m_sequences[ch].is_playing), ch);
		save_item(NAME(m_sequences[ch].is_paused), ch);
		save_item(NAME(m_sequences[ch].offset), ch);
	}
	for (int ch = 0; ch < 8; ch++)
	{
		save_item(NAME(m_sqcs[ch].sqc), ch);
		save_item(NAME(m_sqcs[ch].loop), ch);
		save_item(NAME(m_sqcs[ch].is_playing), ch);
		save_item(NAME(m_sqcs[ch].is_waiting), ch);
		save_item(NAME(m_sqcs[ch].offset), ch);
	}
}


//-------------------------------------------------
//  device_reset - device-specific reset
//-------------------------------------------------

void ymz770_device::device_reset()
{
	for (auto & channel : m_channels)
	{
		channel.phrase = 0;
		channel.pan = 64;
		channel.pan_delay = 0;
		channel.pan1 = 64;
		channel.pan1_delay = 0;
		channel.volume = 0;
		channel.volume_target = 0;
		channel.volume_delay = 0;
		channel.volume2 = 0;
		channel.loop = 0;
		channel.is_playing = false;
		channel.is_paused = false;
		channel.output_remaining = 0;
		channel.decoder->clear();
	}
	for (auto & sequence : m_sequences)
	{
		sequence.delay = 0;
		sequence.sequence = 0;
		sequence.timer = 0;
		sequence.stopchan = 0;
		sequence.loop = 0;
		sequence.bank = 0;
		sequence.is_playing = false;
		sequence.is_paused = false;
	}
	for (auto & sqc : m_sqcs)
	{
		sqc.is_playing = false;
		sqc.loop = 0;
	}
}


//-------------------------------------------------
//  sound_stream_update - handle update requests for
//  our sound stream
//-------------------------------------------------

void ymz770_device::sound_stream_update(sound_stream &stream, stream_sample_t **inputs, stream_sample_t **outputs, int samples)
{
	stream_sample_t *outL, *outR;

	outL = outputs[0];
	outR = outputs[1];

	for (int i = 0; i < samples; i++)
	{
		sequencer();

		// process channels
		int32_t mixl = 0;
		int32_t mixr = 0;

		for (auto & channel : m_channels)
		{
			if (channel.output_remaining > 0)
			{
				// force finish current block
				int32_t smpl = ((int32_t)channel.output_data[channel.output_ptr++] * (channel.volume >> 17)) >> 7;   // volume is linear, 0 - 128 (100%)
				smpl = (smpl * channel.volume2) >> 7;
				mixr += (smpl * channel.pan) >> 7;  // pan seems linear, 0 - 128, where 0 = 100% left, 128 = 100% right, 64 = 50% left 50% right
				mixl += (smpl * (128 - channel.pan)) >> 7;
				channel.output_remaining--;

				if (channel.output_remaining == 0 && !channel.is_playing)
					channel.decoder->clear();
			}

			else if (channel.is_playing && !channel.is_paused)
			{
retry:
				if (channel.last_block)
				{
					if (channel.loop)
					{
						if (channel.loop != 255)
							--channel.loop;
						// loop sample
						int phrase = channel.phrase;
						channel.atbl = m_rom[(4*phrase)+0] >> 4 & 7;
						channel.pptr = 8 * get_phrase_offs(phrase);
					}
					else
					{
						channel.is_playing = false;
						channel.output_remaining = 0;
						channel.decoder->clear();
					}
				}

				if (channel.is_playing)
				{
					// next block
					int sample_rate, channel_count;
					if (!channel.decoder->decode_buffer(channel.pptr, m_rom.bytes()*8, channel.output_data, channel.output_remaining, sample_rate, channel_count) || channel.output_remaining == 0)
					{
						channel.is_playing = !channel.last_block; // detect infinite retry loop
						channel.last_block = true;
						channel.output_remaining = 0;
						goto retry;
					}

					channel.last_block = channel.output_remaining < 1152;
					channel.output_remaining--;
					channel.output_ptr = 1;

					int32_t smpl = ((int32_t)channel.output_data[0] * (channel.volume >> 17)) >> 7;
					smpl = (smpl * channel.volume2) >> 7;
					mixr += (smpl * channel.pan) >> 7;
					mixl += (smpl * (128 - channel.pan)) >> 7;
				}
			}
		}

		mixr *= m_vlma; // main volume is linear, 0 - 255, where 128 = 100%
		mixl *= m_vlma;
		mixr >>= 7 - m_bsl;
		mixl >>= 7 - m_bsl;
		// Clip limiter: 0 - off, 1 - 6.02 dB (100%), 2 - 4.86 dB (87.5%), 3 - 3.52 dB (75%)
		constexpr int32_t ClipMax3 = 32768 * 75 / 100;
		constexpr int32_t ClipMax2 = 32768 * 875 / 1000;
		switch (m_cpl)
		{
		case 3:
			mixl = (mixl > ClipMax3) ? ClipMax3 : (mixl < -ClipMax3) ? -ClipMax3 : mixl;
			mixr = (mixr > ClipMax3) ? ClipMax3 : (mixr < -ClipMax3) ? -ClipMax3 : mixr;
			break;
		case 2:
			mixl = (mixl > ClipMax2) ? ClipMax2 : (mixl < -ClipMax2) ? -ClipMax2 : mixl;
			mixr = (mixr > ClipMax2) ? ClipMax2 : (mixr < -ClipMax2) ? -ClipMax2 : mixr;
			break;
		case 1:
			mixl = (mixl > 32767) ? 32767 : (mixl < -32768) ? -32768 : mixl;
			mixr = (mixr > 32767) ? 32767 : (mixr < -32768) ? -32768 : mixr;
			break;
		}
		if (m_mute)
			mixr = mixl = 0;
		outL[i] = mixl;
		outR[i] = mixr;
	}
}

void ymz770_device::sequencer()
{
	for (auto & sequence : m_sequences)
	{
		if (sequence.is_playing)
		{
			if (sequence.delay > 0)
				sequence.delay--;
			else
			{
				int reg = get_rom_byte(sequence.offset++);
				uint8_t data = get_rom_byte(sequence.offset++);
				switch (reg)
				{
				case 0x0f:
					if (sequence.loop)
						sequence.offset = get_seq_offs(sequence.sequence); // loop sequence
					else
						sequence.is_playing = false;
					break;
				case 0x0e:
					sequence.delay = 32 - 1;
					break;
				default:
					internal_reg_write(reg, data);
					break;
				}
			}
		}
	}
}

//-------------------------------------------------
//  write - write to the chip's registers
//-------------------------------------------------

WRITE8_MEMBER( ymz770_device::write )
{
	if (offset & 1)
	{
		m_stream->update();
		internal_reg_write(m_cur_reg, data);
	}
	else
		m_cur_reg = data;
}


void ymz770_device::internal_reg_write(uint8_t reg, uint8_t data)
{
	// global registers
	if (reg < 0x40)
	{
		switch (reg)
		{
			case 0x00:
				m_mute = data & 1;
				m_doen = data >> 1 & 1;
				break;

			case 0x01:
				m_vlma = data;
				break;

			case 0x02:
				m_bsl = data & 7;
				m_cpl = data >> 4 & 7;
				break;

			// unused
			default:
				logerror("unimplemented write %02X %02X\n", reg, data);
				break;
		}
	}

	// playback registers
	else if (reg < 0x60)
	{
		int ch = reg >> 2 & 0x07;

		switch (reg & 0x03)
		{
			case 0:
				m_channels[ch].phrase = data;
				break;

			case 1:
				m_channels[ch].volume2 = data;
				m_channels[ch].volume = 128 << 17;
				break;

			case 2:
				m_channels[ch].pan = data << 3;
				break;

			case 3:
				if ((data & 6) == 2 || ((data & 6) == 6 && !m_channels[ch].is_playing)) // both KON bits is 1 = "Keep Playing", do not restart channel in this case
				{
					uint8_t phrase = m_channels[ch].phrase;
					m_channels[ch].atbl = m_rom[(4*phrase)+0] >> 4 & 7;
					m_channels[ch].pptr = 8 * get_phrase_offs(phrase);
					m_channels[ch].last_block = false;

					m_channels[ch].is_playing = true;
				}
				else if ((data & 6) == 0)
					m_channels[ch].is_playing = false;

				m_channels[ch].loop = (data & 1) ? 255 : 0;
				break;
		}
	}

	// sequencer registers
	else if (reg >= 0x80)
	{
		int ch = reg >> 4 & 0x07;

		switch (reg & 0x0f)
		{
			case 0:
				m_sequences[ch].sequence = data;
				break;
			case 1:
				if ((data & 6) == 2 || ((data & 6) == 6 && !m_sequences[ch].is_playing)) // both KON bits 1 is "Keep Playing"
				{
					m_sequences[ch].offset = get_seq_offs(m_sequences[ch].sequence);
					m_sequences[ch].delay = 0;
					m_sequences[ch].is_playing = true;
				}
				else if ((data & 6) == 0)
					m_sequences[ch].is_playing = false;

				m_sequences[ch].loop = data & 1;
				break;

			default:
				if (data)
					logerror("unimplemented write %02X %02X\n", reg, data);
				break;
		}
	}
	else
		logerror("unimplemented write %02X %02X\n", reg, data);
}

//-------------------------------------------------
//  ymz774_device
//-------------------------------------------------

ymz774_device::ymz774_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: ymz770_device(mconfig, YMZ774, tag, owner, clock, 44100)
{
}

// volume increments, fractions of 0x20000, likely typical for Yamaha log-linear
static const uint32_t volinc[256] = {
	0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
	32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,
	64,66,68,70,72,74,76,78,80,82,84,86,88,90,92,94,96,98,100,102,104,106,108,110,112,114,116,118,120,122,124,126,
	128,132,136,140,144,148,152,156,160,164,168,172,176,180,184,188,192,196,200,204,208,212,216,220,224,228,232,236,240,244,248,252,
	256,264,272,280,288,296,304,312,320,328,336,344,351,360,368,376,384,392,400,408,416,424,432,440,448,456,464,471,480,488,495,504,
	511,528,543,559,576,592,608,624,639,656,671,688,703,719,736,752,767,783,799,815,831,847,863,879,895,910,928,942,958,975,991,1006,
	1023,1054,1087,1119,1149,1181,1215,1247,1277,1312,1340,1373,1404,1436,1469,1504,1534,1566,1598,1626,1661,1691,1721,1753,1786,1820,1856,1883,1912,1951,1981,2013,
	2045,2102,2174,2238,2292,2363,2423,2487,2553,2624,2679,2737,2797,2860,2926,2996,3068,3118,3197,3252,3308,3367,3427,3490,3555,3623,3694,3767,3804,3882,3963,4005
};

READ8_MEMBER(ymz774_device::read)
{
	if (offset & 1)
	{
		if (m_cur_reg == 0xe3 || m_cur_reg == 0xe4)
		{
			m_stream->update();
			uint8_t res = 0;
			int bank = (m_cur_reg == 0xe3) ? 8 : 0;
			for (int i = 0; i < 8; i++)
				if (m_channels[i + bank].is_playing)
					res |= 1 << i;
			return res;
		}
	}
	logerror("unimplemented read %02X\n", m_cur_reg);
	return 0;
}

void ymz774_device::internal_reg_write(uint8_t reg, uint8_t data)
{
	// playback registers
	if (reg < 0x10)  // phrase num H and L
	{
		int ch = ((reg >> 1) & 7) + m_bank * 8;
		if (reg & 1)
			m_channels[ch].phrase = (m_channels[ch].phrase & 0xff00) | data;
		else
			m_channels[ch].phrase = (m_channels[ch].phrase & 0x00ff) | ((data & 7) << 8);
	}
	else if (reg < 0x60)
	{
		int ch = (reg & 7) + m_bank * 8;
		switch (reg & 0xf8)
		{
		case 0x10: // Volume 1
			m_channels[ch].volume_target = data;
			break;
		case 0x18: // Volume 1 delayed transition
			m_channels[ch].volume_delay = data;
			break;
		case 0x20: // Volume 2
			m_channels[ch].volume2 = data;
			break;
		case 0x28: // Pan L/R
			m_channels[ch].pan = data;
			break;
		case 0x30: // Pan L/R delayed transition
			if (data) logerror("unimplemented write %02X %02X\n", reg, data);
			m_channels[ch].pan_delay = data;
			break;
		case 0x38: // Pan T/B
			m_channels[ch].pan1 = data;
			break;
		case 0x40: // Pan T/B delayed transition
			if (data) logerror("unimplemented write %02X %02X\n", reg, data);
			m_channels[ch].pan1_delay = data;
			break;
		case 0x48: // Loop
			m_channels[ch].loop = data;
			break;
		case 0x50: // Start / Stop
			if (data)
			{
				int phrase = m_channels[ch].phrase;
				m_channels[ch].atbl = m_rom[(4 * phrase) + 0] >> 4 & 7;
				m_channels[ch].pptr = 8 * get_phrase_offs(phrase);
				m_channels[ch].last_block = false;

				m_channels[ch].is_playing = true;
				m_channels[ch].is_paused = false; // checkme, might be not needed
			}
			else
				m_channels[ch].is_playing = false;
			break;
		case 0x58: // Pause / Resume
			m_channels[ch].is_paused = data ? true : false;
			if (data) logerror("CHECKME: CHAN pause/resume %02X %02X\n", reg, data);
			break;
		}
	}
	else if (reg < 0xd0)
	{
		if (m_bank == 0)
		{
			int sq = reg & 7;
			switch (reg & 0xf8)
			{
			case 0x60: // sequence num H and L
			case 0x68:
				sq = (reg >> 1) & 7;
				if (reg & 1)
					m_sequences[sq].sequence = (m_sequences[sq].sequence & 0xff00) | data;
				else
					m_sequences[sq].sequence = (m_sequences[sq].sequence & 0x00ff) | ((data & 0x07) << 8);
				break;
			case 0x70: // Start / Stop
				if (data)
				{
					//logerror("SEQ %d start (%s)\n", sq, m_sequences[sq].is_playing ? "playing":"stopped");
					m_sequences[sq].offset = get_seq_offs(m_sequences[sq].sequence);
					m_sequences[sq].delay = 0;
					m_sequences[sq].is_playing = true;
					m_sequences[sq].is_paused = false; // checkme, might be not needed
				}
				else
				{
					//logerror("SEQ %d stop (%s)\n", sq, m_sequences[sq].is_playing ? "playing" : "stopped");
					if (m_sequences[sq].is_playing)
						for (int ch = 0; ch < 16; ch++)
							if (m_sequences[sq].stopchan & (1 << ch))
								m_channels[ch].is_playing = false;
					m_sequences[sq].is_playing = false;
				}
				break;
			case 0x78: // Pause / Resume
				m_sequences[sq].is_paused = data ? true : false;
				if (data) logerror("CHECKME: SEQ pause/resume %02X %02X\n", reg, data);
				break;
			case 0x80: // Loop count, 0 = off, 255 - infinite
				m_sequences[sq].loop = data;
				break;
			case 0x88: // timer H and L
			case 0x90:
				sq = (reg - 0x88) >> 1;
				if (reg & 1)
					m_sequences[sq].timer = (m_sequences[sq].timer & 0xff00) | data;
				else
					m_sequences[sq].timer = (m_sequences[sq].timer & 0x00ff) | (data << 8);
				break;
			case 0x98: // Off trigger, bit4 = on/off, bits0-3 channel (end sequence when channel playback ends)
				if (data) logerror("SEQ Off trigger unimplemented %02X %02X\n", reg, data);
				break;
			case 0xa0: // stop channel mask H and L (when sequence stopped)
			case 0xa8:
				sq = (reg >> 1) & 7;
				if (reg & 1)
					m_sequences[sq].stopchan = (m_sequences[sq].stopchan & 0xff00) | data;
				else
					m_sequences[sq].stopchan = (m_sequences[sq].stopchan & 0x00ff) | (data << 8);
				break;
			case 0xb0: // SQC number
				m_sqcs[sq].sqc = data;
				break;
			case 0xb8: // SQC start / stop
				if (data)
				{
					//logerror("SQC %d start (%s)\n", sq, m_sqcs[sq].is_playing ? "playing" : "stopped");
					m_sqcs[sq].offset = get_sqc_offs(m_sqcs[sq].sqc);
					m_sqcs[sq].is_playing = true;
					m_sqcs[sq].is_waiting = false;
				}
				else
				{
					//logerror("SQC %d stop (%s)\n", sq, m_sqcs[sq].is_playing ? "playing" : "stopped");
					m_sqcs[sq].is_playing = false;
					// stop SEQ too, and stop channels
					if (m_sequences[sq].is_playing)
						for (int ch = 0; ch < 16; ch++)
							if (m_sequences[sq].stopchan & (1 << ch))
								m_channels[ch].is_playing = false;
					m_sequences[sq].is_playing = false;
				}
				break;
			case 0xc0: // SQC loop (255 = infinite)
				m_sqcs[sq].loop = data;
				break;
			default:
				logerror("unimplemented write %02X %02X\n", reg, data);
				break;
			}
		}
		// else bank1 - Equalizer control
	}
	// global registers
	else
	{
		switch (reg) {
		case 0xd0: // Total Volume L0/R0
			m_vlma = data;
			break;
		case 0xd1: // Total Volume L1/R1
			m_vlma1 = data;
			break;
		case 0xd2: // Clip limit
			m_cpl = data;
			break;
		//case 0xd3: // Digital/PWM output
		//case 0xd4: // Digital audio IF/IS L0/R0
		//case 0xd5: // Digital audio IF/IS L1/R1
		//case 0xd8: // GPIO A
		//case 0xdd: // GPIO B
		//case 0xde: // GPIO C
		case 0xf0:
			m_bank = data & 1;
			if (data > 1) logerror("Set bank %02X!\n", data);
			break;
		default:
			logerror("unimplemented write %02X %02X\n", reg, data);
			break;
		}
	}
}

void ymz774_device::sequencer()
{
	for (auto & chan : m_channels)
	{
		if (chan.is_playing && !chan.is_paused && (chan.volume >> 17) != chan.volume_target)
		{
			if (chan.volume_delay)
			{
				if ((chan.volume >> 17) < chan.volume_target)
					chan.volume += volinc[chan.volume_delay];
				else
					chan.volume -= volinc[chan.volume_delay];
			}
			else
				chan.volume = chan.volume_target << 17;
		}
	}

	for (int i = 0; i < 8; i++)
	{
		auto & sqc = m_sqcs[i];
		auto & sequence = m_sequences[i];

		if (sqc.is_playing && !sqc.is_waiting)
		{
			// SQC consists of 4 byte records: SEQ num H, SEQ num L, SEQ Loop count, End flag (0xff)
			sequence.sequence = ((get_rom_byte(sqc.offset) << 8) | get_rom_byte(sqc.offset + 1)) & 0x7ff;
			sqc.offset += 2;
			sequence.loop = get_rom_byte(sqc.offset++);
			sequence.offset = get_seq_offs(sequence.sequence);
			sequence.delay = 0;
			sequence.is_playing = true;
			sequence.is_paused = false; // checkme, might be not needed
			sqc.is_waiting = true;
			if (get_rom_byte(sqc.offset++) == 0xff)
			{
				if (sqc.loop)
				{
					if (sqc.loop != 255)
						--sqc.loop;
					sqc.offset = get_sqc_offs(sqc.sqc);
				}
				else
					sqc.is_playing = false;
			}
		}

		if (sequence.is_playing && !sequence.is_paused)
		{
			if (sequence.delay > 0)
				--sequence.delay;
			else
			{
				int reg = get_rom_byte(sequence.offset++);
				uint8_t data = get_rom_byte(sequence.offset++);
				switch (reg)
				{
				case 0xff: // end
					for (int ch = 0; ch < 16; ch++) // might be wrong, ie not needed in case of loop
						if (sequence.stopchan & (1 << ch))
							m_channels[ch].is_playing = false;
					if (sequence.loop)
					{
						if (sequence.loop != 255)
							--sequence.loop;
						sequence.offset = get_seq_offs(sequence.sequence);
					}
					else
					{
						sequence.is_playing = false;
						sqc.is_waiting = false;
					}
					break;
				case 0xfe: // timer delay
					sequence.delay = sequence.timer * 32; // possible needed -1 or +(32-1)
					break;
				case 0xf0:
					sequence.bank = data & 1;
					break;
				default:
				{
					uint8_t temp = m_bank;
					m_bank = sequence.bank;
					if (m_bank == 0 && reg >= 0x60 && reg < 0xb0) // if we hit SEQ registers need to add this sequence offset
					{
						int sqn = i;
						if (reg < 0x70 || (reg >= 0x88 && reg < 0x98) || reg >= 0xa0)
							sqn = i * 2;
						internal_reg_write(reg + sqn, data);
					}
					else
						internal_reg_write(reg, data);
					m_bank = temp;
				}
					break;
				}
			}
		}
	}
}
