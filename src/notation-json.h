#pragma once
// notation-json.h: the transcribe result as JSON
//
// The ABC text and the real-time timeline of the same transcription in one
// object, so a client aligns lyrics to the note times the transcriber heard
// instead of deriving them from the ABC and a single global tempo. Shared by
// yue-server (/transcribe job result) and yue-transcribe (--json).
//
// Schema:
//   {
//     "abc": string,
//     "duration": seconds,
//     "notes":    [ { "start", "end", "pitch", "track" } ],       track 0 vocal, 1 ins
//     "beats":    [ { "time", "beat_id", "num", "den" } ],
//     "measures": [ { "start_beat", "end_beat", "start", "end",
//                     "numerator", "denominator",
//                     "abc_numerator", "abc_denominator", "pad_before" } ],
//     "keys":     [ { "start", "end", "label" } ],
//     "chords":   [ { "start", "end", "label" } ],
//     "sections": [ { "start", "end", "label" } ]
//   }
// The measures index the beats array (start_beat/end_beat) and repeat the
// span in seconds for convenience. notes are ordered like the cleaned notes
// that become the ABC voices: filter by track and index i is the i-th note
// of that voice.

#include "notation.h"
#include "yyjson.h"

#include <cstdlib>
#include <string>

static void notation_json_intervals(yyjson_mut_doc * doc,
                                    yyjson_mut_val * root,
                                    const char *     key,
                                    const std::vector<NotInterval> & rows) {
    yyjson_mut_val * arr = yyjson_mut_arr(doc);
    for (const NotInterval & r : rows) {
        yyjson_mut_val * item = yyjson_mut_arr_add_obj(doc, arr);
        yyjson_mut_obj_add_real(doc, item, "start", r.start);
        yyjson_mut_obj_add_real(doc, item, "end", r.end);
        yyjson_mut_obj_add_strncpy(doc, item, "label", r.label.c_str(), r.label.size());
    }
    yyjson_mut_obj_add_val(doc, root, key, arr);
}

// The one transcribe result object, the ABC plus its real-time timeline
static std::string notation_json(const std::string & abc, const NotTimeline & timeline) {
    yyjson_mut_doc * doc  = yyjson_mut_doc_new(NULL);
    yyjson_mut_val * root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);

    yyjson_mut_obj_add_strncpy(doc, root, "abc", abc.c_str(), abc.size());
    yyjson_mut_obj_add_real(doc, root, "duration", timeline.duration);

    yyjson_mut_val * notes = yyjson_mut_arr(doc);
    for (const NotTimelineNote & n : timeline.notes) {
        yyjson_mut_val * item = yyjson_mut_arr_add_obj(doc, notes);
        yyjson_mut_obj_add_real(doc, item, "start", n.start);
        yyjson_mut_obj_add_real(doc, item, "end", n.end);
        yyjson_mut_obj_add_int(doc, item, "pitch", n.pitch);
        yyjson_mut_obj_add_int(doc, item, "track", n.track);
    }
    yyjson_mut_obj_add_val(doc, root, "notes", notes);

    yyjson_mut_val * beats = yyjson_mut_arr(doc);
    for (const NotBeat & b : timeline.beats) {
        yyjson_mut_val * item = yyjson_mut_arr_add_obj(doc, beats);
        yyjson_mut_obj_add_real(doc, item, "time", b.time);
        yyjson_mut_obj_add_int(doc, item, "beat_id", b.beat_id);
        yyjson_mut_obj_add_int(doc, item, "num", b.num);
        yyjson_mut_obj_add_int(doc, item, "den", b.den);
    }
    yyjson_mut_obj_add_val(doc, root, "beats", beats);

    yyjson_mut_val * measures = yyjson_mut_arr(doc);
    for (const NotMeasure & m : timeline.measures) {
        yyjson_mut_val * item = yyjson_mut_arr_add_obj(doc, measures);
        yyjson_mut_obj_add_int(doc, item, "start_beat", m.start_beat);
        yyjson_mut_obj_add_int(doc, item, "end_beat", m.end_beat);
        double start = 0.0, end = timeline.duration;
        if (m.start_beat >= 0 && (size_t) m.start_beat < timeline.beats.size()) {
            start = timeline.beats[(size_t) m.start_beat].time;
        }
        if (m.end_beat >= 0 && (size_t) m.end_beat < timeline.beats.size()) {
            end = timeline.beats[(size_t) m.end_beat].time;
        } else if (!timeline.beats.empty()) {
            end = timeline.beats.back().time;
        }
        yyjson_mut_obj_add_real(doc, item, "start", start);
        yyjson_mut_obj_add_real(doc, item, "end", end);
        yyjson_mut_obj_add_int(doc, item, "numerator", m.numerator);
        yyjson_mut_obj_add_int(doc, item, "denominator", m.denominator);
        yyjson_mut_obj_add_int(doc, item, "abc_numerator", m.abc_numerator);
        yyjson_mut_obj_add_int(doc, item, "abc_denominator", m.abc_denominator);
        yyjson_mut_obj_add_bool(doc, item, "pad_before", m.pad_before);
    }
    yyjson_mut_obj_add_val(doc, root, "measures", measures);

    notation_json_intervals(doc, root, "keys", timeline.keys);
    notation_json_intervals(doc, root, "chords", timeline.chords);
    notation_json_intervals(doc, root, "sections", timeline.sections);

    char *      json = yyjson_mut_write(doc, 0, NULL);
    std::string out  = json ? json : "{}";
    if (json) {
        free(json);
    }
    yyjson_mut_doc_free(doc);
    return out;
}