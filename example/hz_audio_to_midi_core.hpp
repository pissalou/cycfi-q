#if !defined(HZ_AUDIO_TO_MIDI_CORE_HPP)
#define HZ_AUDIO_TO_MIDI_CORE_HPP

#include <q/fx/envelope.hpp>
#include <q/pitch/pitch_detector.hpp>
#include <q/support/literals.hpp>
#include <q/support/midi_messages.hpp>
#include <algorithm>
#include <cmath>
#include <cstddef>

namespace hz_audio_to_midi
{
   namespace q = cycfi::q;
   namespace midi = q::midi_1_0;
   using namespace q::literals;

   struct midi_event
   {
      int status;
      int key;
      int velocity;
      std::size_t sample;
   };

   template <typename Sink>
   class processor
   {
   public:
      processor(float sps, Sink& sink)
       : _sink(sink)
       , _detector(55_Hz, 1000_Hz, sps, -45_dB)
       , _envelope(100_ms, sps)
       , _threshold(q::lin_float(-55_dB))
      {}

      void operator()(float sample)
      {
         auto level = _envelope(std::abs(sample));
         if (level >= _threshold)
         {
            if (_detector(sample))
               set_note(_detector.get_frequency());
         }
         else if (_current_note >= 0)
         {
            send(midi::status::note_off, _current_note, 0);
            _current_note = -1;
            _detector.reset();
         }
         ++_sample;
      }

      void release_note()
      {
         if (_current_note >= 0)
         {
            send(midi::status::note_off, _current_note, 0);
            _current_note = -1;
         }
      }

   private:
      void send(int status, int key, int velocity)
      {
         _sink.send({ status, key, velocity, _sample });
      }

      void set_note(float frequency)
      {
         if (frequency <= 0.0f)
            return;

         auto key = std::clamp(
            int(std::lround(69.0 + 12.0 * std::log2(frequency / 440.0)))
          , 0, 127
         );

         if (key != _current_note)
         {
            if (_current_note >= 0)
               send(midi::status::note_off, _current_note, 0);
            send(midi::status::note_on, key, 100);
            _current_note = key;
         }
      }

      Sink&                      _sink;
      q::pitch_detector          _detector;
      q::peak_envelope_follower  _envelope;
      float const                _threshold;
      std::size_t                _sample = 0;
      int                        _current_note = -1;
   };
}

#endif