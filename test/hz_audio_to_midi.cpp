#define CATCH_CONFIG_MAIN
#include <infra/catch.hpp>
#include <q_io/audio_file.hpp>
#include "../example/hz_audio_to_midi_core.hpp"
#include <array>
#include <vector>

namespace q = cycfi::q;

struct recording_midi
{
   void send(hz_audio_to_midi::midi_event const& event)
   {
      events.push_back(event);
   }

   std::vector<hz_audio_to_midi::midi_event> events;
};

TEST_CASE("riff_0_5.wav produces expected MIDI notes and durations")
{
   q::wav_reader source{HZ_AUDIO_TO_MIDI_TEST_AUDIO_FILE};
   REQUIRE(source);
   REQUIRE(source.num_channels() == 1);

   std::vector<float> samples(source.length());
   REQUIRE(source.read(samples) == samples.size());

   recording_midi output;
   hz_audio_to_midi::processor<recording_midi> convert{source.sps(), output};
   for (auto sample : samples)
      convert(sample);
   convert.release_note();

   struct note_span
   {
      int key;
      float duration;
   };

   std::vector<note_span> notes;
   REQUIRE(output.events.size() % 2 == 0);
   for (std::size_t i = 0; i < output.events.size(); i += 2)
   {
      auto const& on = output.events[i];
      auto const& off = output.events[i + 1];
      REQUIRE(on.status == q::midi_1_0::status::note_on);
      REQUIRE(off.status == q::midi_1_0::status::note_off);
      REQUIRE(off.key == on.key);
      notes.push_back({on.key, float(off.sample - on.sample) / source.sps()});
   }

   constexpr std::array expected = {
      note_span{64, 0.3f}, // wrong note -> E4
      note_span{66, 0.2618f}, // F#4
      note_span{67, 0.0334f}, // wrong note (G4)
      note_span{68, 0.2987f}, // G#4
      note_span{69, 0.2427f}, // A4 
      note_span{68, 0.1493f}, // G#4
      note_span{69, 0.0933f}, // A4
      note_span{68, 0.056f}, // G#4
      note_span{66, 0.3f}, // wrong note -> F#4
      note_span{67, 0.0555f}, // G4 (wrong note)
      note_span{68, 0.3174f}, // G#4
      note_span{69, 0.2986f}, // A4
      note_span{71, 2.1885f},  // B4
      note_span{72, 0.1651f}  // C4 (wrong note)
   };

   REQUIRE(notes.size() == expected.size());
   for (std::size_t i = 0; i != expected.size(); ++i)
   {
      INFO("note index = " << i);
      CHECK(notes[i].key == expected[i].key);
      CHECK(notes[i].duration == Approx(expected[i].duration).margin(0.02f));
   }
}