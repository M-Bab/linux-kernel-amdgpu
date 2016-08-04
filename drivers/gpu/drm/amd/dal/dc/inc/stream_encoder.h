/*
 * stream_encoder.h
 *
 */

#ifndef STREAM_ENCODER_H_
#define STREAM_ENCODER_H_

#include "include/hw_sequencer_types.h"

struct dc_bios;
struct dc_context;
struct dc_crtc_timing;


struct encoder_info_packet {
	bool valid;
	uint8_t hb0;
	uint8_t hb1;
	uint8_t hb2;
	uint8_t hb3;
	uint8_t sb[28];
};

struct encoder_info_frame {
	/* auxiliary video information */
	struct encoder_info_packet avi;
	struct encoder_info_packet gamut;
	struct encoder_info_packet vendor;
	/* source product description */
	struct encoder_info_packet spd;
	/* video stream configuration */
	struct encoder_info_packet vsc;
};

struct encoder_unblank_param {
	struct hw_crtc_timing crtc_timing;
	struct link_settings link_settings;
};

struct encoder_set_dp_phy_pattern_param {
	enum dp_test_pattern dp_phy_pattern;
	const uint8_t *custom_pattern;
	uint32_t custom_pattern_size;
	enum dp_panel_mode dp_panel_mode;
};


struct stream_encoder {
	struct stream_encoder_funcs *funcs;
	struct dc_context *ctx;
	struct dc_bios *bp;
	enum engine_id id;
};

struct stream_encoder_funcs {
	void (*dp_set_stream_attribute)(
		struct stream_encoder *enc,
		struct dc_crtc_timing *crtc_timing);
	void (*hdmi_set_stream_attribute)(
		struct stream_encoder *enc,
		struct dc_crtc_timing *crtc_timing,
		bool enable_audio);
	void (*dvi_set_stream_attribute)(
		struct stream_encoder *enc,
		struct dc_crtc_timing *crtc_timing,
		bool is_dual_link);
	void (*set_mst_bandwidth)(
		struct stream_encoder *enc,
		struct fixed31_32 avg_time_slots_per_mtp);
	void (*update_hdmi_info_packets)(
		struct stream_encoder *enc,
		const struct encoder_info_frame *info_frame);
	void (*stop_hdmi_info_packets)(
		struct stream_encoder *enc);
	void (*update_dp_info_packets)(
		struct stream_encoder *enc,
		const struct encoder_info_frame *info_frame);
	void (*stop_dp_info_packets)(
		struct stream_encoder *enc);
	void (*dp_blank)(
		struct stream_encoder *enc);
	void (*dp_unblank)(
		struct stream_encoder *enc,
		const struct encoder_unblank_param *param);
};

#endif /* STREAM_ENCODER_H_ */
