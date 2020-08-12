#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <lv2/core/lv2.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/urid/urid.h>
#include <lv2/midi/midi.h>
#include <lv2/state/state.h>
#include "ayumi.h"

#define AYUMI_LV2_URI "https://github.com/atsushieno/ayumi-lv2"
#define AYUMI_LV2_ATOM_INPUT_PORT 0
#define AYUMI_LV2_AUDIO_OUT_LEFT 1
#define AYUMI_LV2_AUDIO_OUT_RIGHT 2
#define AYUMI_LV2_MIDI_CC_ENVELOPE_MSB 0x50
#define AYUMI_LV2_MIDI_CC_ENVELOPE_LSB 0x51
#define AYUMI_LV2_MIDI_CC_ENVELOPE_SHAPE 0x52
#define AYUMI_LV2_MIDI_CC_DC 0x53

typedef struct {
	LV2_URID_Map *urid_map;
	LV2_URID midi_event_uri;
	struct ayumi* impl;
	int mixer[3];
	int32_t envelope;
	int32_t pitchbend;
	double sample_rate;
	const char * bundle_path;
	float* ports[3];
	bool note_on_state[3];
} AyumiLV2Handle;

LV2_Handle ayumi_lv2_instantiate(
		const LV2_Descriptor * descriptor,
		double sample_rate,
		const char * bundle_path,
		const LV2_Feature *const * features) {
	AyumiLV2Handle* handle = (AyumiLV2Handle*) calloc(sizeof(AyumiLV2Handle), 1);
	handle->impl = calloc(sizeof(struct ayumi), 1);
	handle->sample_rate = sample_rate;
	handle->bundle_path = strdup(bundle_path);

	/* clock_rate / (sample_rate * 8 * 8) must be < 1.0 */
	ayumi_configure(handle->impl, 1, sample_rate, (int) sample_rate);
	ayumi_set_noise(handle->impl, 4); // pink noise by default
	for (int i = 0; i < 3; i++) {
		handle->mixer[i] = 1 << 7; // FIXME: should be tone only by default
		ayumi_set_pan(handle->impl, i, 0.5, 0); // 0(L)...1(R)
		ayumi_set_volume(handle->impl, i, 15);
		ayumi_set_mixer(handle->impl, i, 1, 1, 1); // should be quiet by default
		ayumi_set_envelope_shape(handle->impl, 10); // see http://fmpdoc.fmp.jp/%E3%82%A8%E3%83%B3%E3%83%99%E3%83%AD%E3%83%BC%E3%83%97%E3%83%8F%E3%83%BC%E3%83%89%E3%82%A6%E3%82%A7%E3%82%A2/
		ayumi_set_envelope(handle->impl, 0x40); // somewhat slow
	}

	handle->urid_map = NULL;
	for (int i = 0; features[i]; i++) {
		const LV2_Feature* f = features[i];
		if (!strcmp(f->URI, LV2_URID__map))
			handle->urid_map = (LV2_URID_Map*) f->data;
	}
	assert(handle->urid_map);
	handle->midi_event_uri = handle->urid_map->map(handle->urid_map->handle, LV2_MIDI__MidiEvent);

	return handle;
}

void ayumi_lv2_connect_port(
		LV2_Handle instance,
		uint32_t port,
		void * data_location) {
	AyumiLV2Handle* a = (AyumiLV2Handle*) instance;
	a->ports[port] = data_location;
}

void ayumi_lv2_activate(LV2_Handle instance) {
}

void ayumi_lv2_process_midi_event(AyumiLV2Handle *a, const LV2_Atom_Event *ev) {
	int noise, tone_switch, noise_switch, env_switch;
	const uint8_t *const msg = (const uint8_t *)(ev + 1);
	int channel = msg[0] & 0xF;
	if (channel > 2)
		return;
	int mixer;
	switch (lv2_midi_message_type(msg)) {
	case LV2_MIDI_MSG_NOTE_OFF: note_off:
		if (!a->note_on_state[channel])
			break; // not at note on state
		ayumi_set_mixer(a->impl, channel, 1, 1, 0);
		a->note_on_state[channel] = false;
		break;
	case LV2_MIDI_MSG_NOTE_ON:
		if (msg[2] == 0)
			goto note_off; // it is illegal though.
		if (a->note_on_state[channel])
			break; // busy
		mixer = a->mixer[channel];
		tone_switch = (mixer >> 5) & 1;
		noise_switch = (mixer >> 6) & 1;
		env_switch = (mixer >> 7) & 1;
		ayumi_set_mixer(a->impl, channel, tone_switch, noise_switch, env_switch);
		ayumi_set_tone(a->impl, channel, msg[1] * 4096 / 128);
		a->note_on_state[channel] = true;
		break;
	case LV2_MIDI_MSG_PGM_CHANGE:
		noise = msg[1] & 0x1F;
		mixer = msg[1];
		tone_switch = (mixer >> 5) & 1;
		noise_switch = (mixer >> 6) & 1;
		env_switch = (mixer >> 7) & 1;
		ayumi_set_mixer(a->impl, channel, tone_switch, noise_switch, env_switch);
		ayumi_set_noise(a->impl, noise);
		break;
	case LV2_MIDI_MSG_CONTROLLER:
		switch (msg[1]) {
		case LV2_MIDI_CTL_MSB_PAN:
			ayumi_set_pan(a->impl, channel, msg[2] / 128.0, 0);
			break;
		case LV2_MIDI_CTL_MSB_MAIN_VOLUME:
			ayumi_set_volume(a->impl, channel, msg[2] / 8);
			break;
		case AYUMI_LV2_MIDI_CC_ENVELOPE_MSB:
			a->envelope = (a->envelope & 0x7F) + (msg[2] << 7);
			ayumi_set_envelope(a->impl, a->envelope);
			break;
		case AYUMI_LV2_MIDI_CC_ENVELOPE_LSB:
			a->envelope = (a->envelope & 0x3F80) + msg[2];
			ayumi_set_envelope(a->impl, a->envelope);
			break;
		case AYUMI_LV2_MIDI_CC_ENVELOPE_SHAPE:
			ayumi_set_envelope_shape(a->impl, msg[2] & 0xF);
			break;
		case AYUMI_LV2_MIDI_CC_DC:
			ayumi_remove_dc(a->impl);
			break;
		}
		break;
	case LV2_MIDI_MSG_BENDER:
		a->pitchbend = (msg[1] << 7) + msg[2];
		break;
	default:
		break;
	}
}

void ayumi_lv2_run(LV2_Handle instance, uint32_t sample_count) {
	AyumiLV2Handle* a = (AyumiLV2Handle*) instance;

	LV2_Atom_Sequence* seq = (LV2_Atom_Sequence*) a->ports[AYUMI_LV2_ATOM_INPUT_PORT];

	LV2_ATOM_SEQUENCE_FOREACH(seq, ev) {
		if (ev->body.type == a->midi_event_uri) {
			puts("MIDI EVENT");
			ayumi_lv2_process_midi_event(a, ev);
		}
	}

	for (int i = 0; i < sample_count; i++) {
		ayumi_process(a->impl);
		a->ports[AYUMI_LV2_AUDIO_OUT_LEFT][i] = (float) a->impl->left;
		a->ports[AYUMI_LV2_AUDIO_OUT_RIGHT][i] = (float) a->impl->right;
	}
}

void ayumi_lv2_deactivate(LV2_Handle instance) {
}

void ayumi_lv2_cleanup(LV2_Handle instance) {
	AyumiLV2Handle* a = (AyumiLV2Handle*) instance;
	free((void*) a->bundle_path);
	free(a->impl);
	free(a);
}

const void * ayumi_lv2_extension_data(const char * uri) {
	return NULL;
}

const LV2_Descriptor ayumi_lv2 = {
	AYUMI_LV2_URI,
	ayumi_lv2_instantiate,
	ayumi_lv2_connect_port,
	ayumi_lv2_activate,
	ayumi_lv2_run,
	ayumi_lv2_deactivate,
	ayumi_lv2_cleanup,
	ayumi_lv2_extension_data
};

const LV2_Descriptor * lv2_descriptor(uint32_t index)
{
	return &ayumi_lv2;
}

