// test-notation.cpp: measure inference from decoded beats
//
// SheetSage2 labels each rhythm event with the eighth position it hears, and
// notation.h turns the events labelled beat 1 into downbeats, so the measures
// are the spans between two downbeats. A weak beat labelled beat 1 at a phrase
// boundary used to become a one-beat measure and the ABC carried a 1/4 meter.
// not_measures() now re-cuts the score from the first span whose length
// disagrees with the meter the beats themselves carry.
//
// Pure CPU: notation.h only needs the standard library.

#include "notation.h"
#include "notation-json.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

static std::vector<NotBeat> grid(const std::vector<int> & beat_ids, int num = 4, int den = 4) {
    std::vector<NotBeat> beats;
    beats.reserve(beat_ids.size());
    for (size_t i = 0; i < beat_ids.size(); i++) {
        beats.push_back({ 0.5 * (double) i, beat_ids[i], num, den });
    }
    return beats;
}

static bool infer(const std::vector<NotBeat> & beats, std::vector<NotMeasure> * measures) {
    std::string error;
    if (!not_measures(beats, measures, &error)) {
        fprintf(stderr, "[Test-Notation] FAIL: not_measures: %s\n", error.c_str());
        return false;
    }
    return true;
}

static bool all_full_bars(const std::vector<NotMeasure> & measures, int num) {
    for (const NotMeasure & m : measures) {
        if (m.end_beat - m.start_beat != num || m.abc_numerator != num || m.abc_denominator != 4) {
            return false;
        }
    }
    return true;
}

static void expect(bool condition, const char * what) {
    if (!condition) {
        fprintf(stderr, "[Test-Notation] FAIL: %s\n", what);
        exit(1);
    }
}

static void expect_span(const NotMeasure & m, int start, int end, const char * what) {
    if (m.start_beat != start || m.end_beat != end) {
        fprintf(stderr, "[Test-Notation] FAIL: %s (measure [%d,%d), expected [%d,%d))\n", what, m.start_beat, m.end_beat, start, end);
        exit(1);
    }
}

// A clean 4/4 score: four beats per measure, the last downbeat closes it.
static void clean_4_4_is_untouched() {
    std::vector<int> ids;
    for (int bar = 0; bar < 8; bar++) {
        ids.push_back(1);
        ids.push_back(2);
        ids.push_back(3);
        ids.push_back(4);
    }
    ids.push_back(1);
    std::vector<NotMeasure> measures;
    expect(infer(grid(ids), &measures), "clean 4/4 reads");
    expect(measures.size() == 8, "clean 4/4 keeps 8 measures");
    for (size_t i = 0; i < measures.size(); i++) {
        expect_span(measures[i], (int) i * 4, (int) i * 4 + 4, "clean 4/4 boundary");
    }
    expect(all_full_bars(measures, 4), "clean 4/4 stays 4/4");
}

// The 王北车-画 pattern: a downbeat one beat after the last one (the model
// labelled a weak beat beat 1), and the new phase keeps going afterwards.
static void early_downbeat_is_rebarred() {
    std::vector<int> ids = { 1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4,
                             1,  // the bar starts here
                             1, 2, 3, 4,  // one beat in, the slip lands
                             1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4 };
    std::vector<NotMeasure> measures;
    expect(infer(grid(ids), &measures), "slipped 4/4 reads");
    expect(measures.size() == 8, "slipped 4/4 has 8 measures");
    for (size_t i = 0; i < 4; i++) {
        expect_span(measures[i], (int) i * 4, (int) i * 4 + 4, "measures before the slip stay");
    }
    expect(all_full_bars(measures, 4), "slipped 4/4 is rebarred to 4/4");
}

// The 借 pattern: three beats then one before the next real downbeat.
static void three_one_pair_is_rebarred() {
    std::vector<int> ids = { 1, 2, 3, 4, 1, 2, 3, 1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4, 1, 2 };
    // downbeats 0, 4, 7, 11, 15, 19: spans 4, 3, 4, 4, 4
    std::vector<NotMeasure> measures;
    expect(infer(grid(ids), &measures), "3+1 reads");
    expect(measures.size() == 5, "3+1 rebarred into 5 measures");
    expect(all_full_bars(measures, 4), "3+1 becomes 4/4");
}

// A short first span is a pickup: it keeps its notes and takes the following
// meter with a leading rest.
static void pickup_is_padded() {
    std::vector<int> ids = { 3, 4, 1, 2, 3, 4, 1, 2, 3, 4, 1 };
    std::vector<NotMeasure> measures;
    expect(infer(grid(ids), &measures), "pickup reads");
    expect(measures.size() == 3, "pickup plus two measures");
    expect(measures[0].pad_before, "pickup is padded before");
    expect(measures[0].abc_numerator == 4 && measures[0].abc_denominator == 4, "pickup takes the following meter");
    expect(measures[0].end_beat - measures[0].start_beat == 2, "pickup keeps its two beats");
}

// A real 3/4 score must not be rebarred to 4/4.
static void waltz_stays_three() {
    std::vector<int> ids;
    for (int bar = 0; bar < 8; bar++) {
        ids.push_back(1);
        ids.push_back(2);
        ids.push_back(3);
    }
    ids.push_back(1);
    std::vector<NotMeasure> measures;
    expect(infer(grid(ids, 3, 4), &measures), "3/4 reads");
    expect(measures.size() == 8, "3/4 keeps 8 measures");
    expect(all_full_bars(measures, 3), "3/4 stays 3/4");
}

// Beats that disagree about the meter are a meter change, not a slip: the
// span is left alone.
static void mixed_meter_span_is_kept() {
    std::vector<NotBeat> beats = {
        { 0.0, 1, 4, 4 }, { 0.5, 2, 4, 4 }, { 1.0, 3, 4, 4 }, { 1.5, 4, 4, 4 },
        { 2.0, 1, 4, 4 }, { 2.5, 2, 3, 4 }, { 3.0, 3, 3, 4 }, { 3.5, 4, 3, 4 },
        { 4.0, 1, 3, 4 },
    };
    std::vector<NotMeasure> measures;
    expect(infer(beats, &measures), "mixed meter reads");
    expect(measures.size() == 2, "mixed meter keeps both spans");
    expect_span(measures[0], 0, 4, "first span unchanged");
    expect_span(measures[1], 4, 8, "mixed span unchanged");
}

// A four measure 4/4 song at 120 BPM with two voices: the vocal on every one
// of the first eight beats, the instrumental on every fourth. One extra note
// is shorter than a MIDI tick, so it becomes no ABC note and must not show up
// in the timeline either.
static std::vector<NotEvent> song_events() {
    std::vector<NotEvent> events;
    for (int bar = 0; bar < 4; bar++) {
        for (int beat = 0; beat < 4; beat++) {
            int      index = bar * 4 + beat;
            NotEvent e;
            e.time           = 0.5 * index;
            e.subbeat        = 0;
            e.global_subbeat = index;
            e.has_meter      = true;
            e.meter_num      = 4;
            e.meter_den      = 4;
            e.eighth         = beat * 2;
            if (index == 0) {
                e.key       = "C:maj";
                e.chord     = "C:maj";
                e.structure = "verse";
            }
            events.push_back(e);
        }
    }
    for (int index = 0; index < 8; index++) {
        events[(size_t) index].melody.push_back({ 60 + index, 0, 1, events[(size_t) index].time + 0.4 });
    }
    for (int index : { 2, 6, 10, 14 }) {
        events[(size_t) index].melody.push_back({ 48, 1, 1, events[(size_t) index].time + 0.4 });
    }
    // Shorter than the 1920 tick round trip: dropped by the ABC, dropped here
    events[9].melody.push_back({ 62, 0, 1, events[9].time + 0.0001 });
    return events;
}

// The real-time timeline describes the same transcription as the ABC: one
// note per ABC onset of each voice, the same beats and measures, the
// intervals in the song domain.
static void timeline_matches_the_score() {
    const double duration = 8.0;
    NotTables    tables;
    tables.key_abc["C:maj"]   = "C";
    tables.chord_abc["C:maj"] = "C";

    std::vector<NotEvent> events = song_events();
    NotScore              score;
    NotTimeline           timeline;
    std::string           error;
    if (!notation_build(events, duration, tables, false, &score, &timeline, &error)) {
        fprintf(stderr, "[Test-Notation] FAIL: notation_build: %s\n", error.c_str());
        exit(1);
    }

    expect(timeline.duration == duration, "timeline duration is the song duration");
    expect(score.beats.size() == timeline.beats.size(), "timeline beat count matches the score");
    for (size_t i = 0; i < timeline.beats.size(); i++) {
        expect(timeline.beats[i].time == score.beats[i].time, "timeline beat time matches the score");
        expect(timeline.beats[i].beat_id == score.beats[i].beat_id, "timeline beat id matches the score");
    }
    expect(timeline.beats.size() > 1, "timeline carries the extended beats");
    for (size_t i = 1; i < timeline.beats.size(); i++) {
        expect(timeline.beats[i].time > timeline.beats[i - 1].time, "timeline beats are strictly increasing");
    }

    // Measures index the beats array and tile it
    expect(timeline.measures.size() == score.measures.size(), "timeline measure count matches the score");
    expect(!timeline.measures.empty(), "timeline carries measures");
    for (size_t i = 0; i < timeline.measures.size(); i++) {
        const NotMeasure & m = timeline.measures[i];
        expect(m.start_beat >= 0 && m.end_beat > m.start_beat, "measure span is positive");
        expect((size_t) m.end_beat < timeline.beats.size(), "measure span stays inside the beats");
        expect(m.denominator > 0 && m.numerator > 0, "measure meter is positive");
        expect(m.abc_denominator > 0 && m.abc_numerator > 0, "measure notated meter is positive");
        if (i > 0) {
            expect(m.start_beat == timeline.measures[i - 1].end_beat, "measures tile the beats");
        }
    }

    // One timeline note per ABC onset of each voice, in the same order
    int total = 0;
    for (int track = 0; track < 2; track++) {
        int abc_notes = 0;
        for (int v : score.voice[track]) {
            if (v > 0 && (v % 2) == 1) {
                abc_notes++;
            }
        }
        int    timeline_notes = 0;
        double last_start     = -1.0;
        for (const NotTimelineNote & n : timeline.notes) {
            if (n.track != track) {
                continue;
            }
            timeline_notes++;
            expect(n.end > n.start, "timeline note has a positive span");
            expect(n.start >= 0.0 && n.end <= duration + 1e-9, "timeline note stays inside the song");
            expect(n.start > last_start, "timeline notes of a voice are strictly ordered");
            last_start = n.start;
        }
        expect(timeline_notes == abc_notes, "timeline note count matches the ABC voice");
        total += abc_notes;
    }
    expect(total == 12, "the song has twelve notatable notes");
    expect((int) timeline.notes.size() == total, "the tick-collapsed note is dropped from the timeline");

    // The interval fields, in real seconds
    expect(timeline.keys.size() == 1 && timeline.keys[0].label == "C", "timeline key is the ABC spelling");
    expect(timeline.keys[0].start == 0.0 && timeline.keys[0].end == duration, "key spans the song");
    expect(timeline.chords.size() == 1 && timeline.chords[0].label == "C:maj", "timeline chord keeps its label");
    expect(timeline.sections.size() == 1 && timeline.sections[0].label == "verse", "timeline section keeps its label");
    for (const NotInterval & r : timeline.keys) {
        expect(r.end > r.start, "timeline interval has a positive span");
    }

    // The ABC is untouched by the timeline request
    std::string abc;
    expect(notation_abc(events, duration, tables, false, &abc, &error), "ABC still renders");
    expect(abc.find("V: Vocal") != std::string::npos, "ABC carries the vocal voice");
    expect(abc.find("V: Ins") != std::string::npos, "ABC carries the instrumental voice");
}

// The JSON the server and the CLI emit carries the ABC and the timeline in
// one object, with the field names the clients read.
static void timeline_json_has_the_schema() {
    const double duration = 8.0;
    NotTables    tables;
    tables.key_abc["C:maj"]   = "C";
    tables.chord_abc["C:maj"] = "C";

    std::vector<NotEvent> events = song_events();
    NotScore              score;
    NotTimeline           timeline;
    std::string           error;
    expect(notation_build(events, duration, tables, false, &score, &timeline, &error), "timeline build for JSON");
    std::string abc;
    expect(notation_abc(events, duration, tables, false, &abc, &error), "ABC build for JSON");

    std::string json = notation_json(abc, timeline);
    yyjson_doc * doc = yyjson_read(json.c_str(), json.size(), 0);
    if (!doc) {
        fprintf(stderr, "[Test-Notation] FAIL: emitted JSON does not parse\n");
        exit(1);
    }
    yyjson_val * root = yyjson_doc_get_root(doc);
    expect(yyjson_is_obj(root) != 0, "timeline JSON is an object");
    expect(yyjson_is_str(yyjson_obj_get(root, "abc")) != 0, "timeline JSON carries the ABC");
    yyjson_val * dur = yyjson_obj_get(root, "duration");
    expect(yyjson_is_num(dur) != 0 && std::fabs(yyjson_get_real(dur) - duration) < 1e-9,
           "timeline JSON carries the duration");

    yyjson_val * notes = yyjson_obj_get(root, "notes");
    expect(yyjson_is_arr(notes) != 0 && yyjson_arr_size(notes) == timeline.notes.size(), "timeline JSON notes");
    yyjson_val * note = yyjson_arr_get_first(notes);
    expect(yyjson_is_obj(note) != 0, "timeline JSON note is an object");
    for (const char * field : { "start", "end", "pitch", "track" }) {
        expect(yyjson_obj_get(note, field) != nullptr, "timeline JSON note field");
    }

    yyjson_val * beats = yyjson_obj_get(root, "beats");
    expect(yyjson_is_arr(beats) != 0 && yyjson_arr_size(beats) == timeline.beats.size(), "timeline JSON beats");
    yyjson_val * beat = yyjson_arr_get_first(beats);
    for (const char * field : { "time", "beat_id", "num", "den" }) {
        expect(yyjson_obj_get(beat, field) != nullptr, "timeline JSON beat field");
    }

    yyjson_val * measures = yyjson_obj_get(root, "measures");
    expect(yyjson_is_arr(measures) != 0 && yyjson_arr_size(measures) == timeline.measures.size(),
           "timeline JSON measures");
    yyjson_val * measure = yyjson_arr_get_first(measures);
    for (const char * field : { "start_beat", "end_beat", "start", "end", "numerator", "denominator",
                                "abc_numerator", "abc_denominator", "pad_before" }) {
        expect(yyjson_obj_get(measure, field) != nullptr, "timeline JSON measure field");
    }

    const char * interval_keys[3] = { "keys", "chords", "sections" };
    for (const char * key : interval_keys) {
        yyjson_val * arr = yyjson_obj_get(root, key);
        expect(yyjson_is_arr(arr) != 0, "timeline JSON interval array");
        yyjson_val * interval = yyjson_arr_get_first(arr);
        expect(yyjson_is_obj(interval) != 0, "timeline JSON interval is an object");
        for (const char * field : { "start", "end", "label" }) {
            expect(yyjson_obj_get(interval, field) != nullptr, "timeline JSON interval field");
        }
    }
    yyjson_doc_free(doc);
}

int main() {
    clean_4_4_is_untouched();
    early_downbeat_is_rebarred();
    three_one_pair_is_rebarred();
    pickup_is_padded();
    waltz_stays_three();
    mixed_meter_span_is_kept();
    timeline_matches_the_score();
    timeline_json_has_the_schema();
    fprintf(stderr, "[Test-Notation] ok\n");
    return 0;
}
