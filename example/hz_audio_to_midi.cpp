/*=============================================================================
   Copyright (c) 2014-2023 Joel de Guzman. All rights reserved.

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <q/support/literals.hpp>
#include "hz_audio_to_midi_core.hpp"
#include <q_io/audio_stream.hpp>
#include <q_io/audio_device.hpp>
#include <q_io/midi_device.hpp>
#include <q/utility/sleep.hpp>
#include <portmidi.h>
#include "example.hpp"
#include <iostream>
#include <vector>

namespace q = cycfi::q;
using namespace q::literals;

struct midi_output
{
   explicit midi_output(int device_id)
   {
      if (Pm_OpenOutput(&_stream, device_id, nullptr, 0, nullptr, nullptr, 0) != pmNoError)
         _stream = nullptr;
   }

   ~midi_output()
   {
      if (_stream)
         Pm_Close(_stream);
   }

   bool is_valid() const
   {
      return _stream != nullptr;
   }

   void send(hz_audio_to_midi::midi_event const& event)
   {
      if (_stream)
         Pm_WriteShort(
            _stream, 0
          , Pm_Message(event.status, event.key, event.velocity)
         );
   }

   PortMidiStream* _stream = nullptr;
};

struct pitch_to_midi : q::audio_stream
{
   pitch_to_midi(q::audio_device const& device, midi_output& output)
    : audio_stream(device, 1, 0)
    , _processor(sampling_rate(), output)
   {}

   void process(in_channels const& in, out_channels const&)
   {
      auto input = in[0];
      for (auto frame : in.frames)
         _processor(input[frame]);
   }

   void release_note()
   {
      _processor.release_note();
   }

private:
   hz_audio_to_midi::processor<midi_output> _processor;
};

int get_midi_output_device()
{
   auto devices = q::midi_device::list();
   std::vector<int> output_ids;

   std::cout << "Available MIDI output devices (ID : Name):\n";
   for (auto const& device : devices)
   {
      if (device.num_outputs() != 0)
      {
         std::cout << device.id() << " : " << device.name() << '\n';
         output_ids.push_back(device.id());
      }
   }

   if (output_ids.empty())
      return -1;

   std::cout << "Choose MIDI output device ID: ";
   int id = -1;
   if (!(std::cin >> id) || std::find(output_ids.begin(), output_ids.end(), id) == output_ids.end())
      return -1;
   return id;
}

int get_audio_input_device(std::vector<q::audio_device> const& devices)
{
   std::vector<int> input_ids;

   std::cout << "Available audio input devices (ID : Name):\n";
   for (auto const& device : devices)
   {
      if (device.input_channels() != 0)
      {
         std::cout << device.id() << " : " << device.name() << '\n';
         input_ids.push_back(device.id());
      }
   }

   if (input_ids.empty())
      return -1;

   std::cout << "Choose audio input device ID: ";
   int id = -1;
   if (!(std::cin >> id) || std::find(input_ids.begin(), input_ids.end(), id) == input_ids.end())
      return -1;
   return id;
}

int main()
{
   signal(SIGINT, signal_handler);
   signal(SIGTERM, signal_handler);

   auto midi_id = get_midi_output_device();
   if (midi_id < 0)
   {
      std::cerr << "No valid MIDI output device selected.\n";
      return 1;
   }

   midi_output output{ midi_id };
   if (!output.is_valid())
   {
      std::cerr << "Could not open the MIDI output device.\n";
      return 1;
   }

   auto devices = q::audio_device::list();
   auto audio_id = get_audio_input_device(devices);
   if (audio_id < 0)
   {
      std::cerr << "No valid audio input device selected.\n";
      return 1;
   }

   auto device = std::find_if(devices.begin(), devices.end(), [audio_id](auto const& item)
   {
      return item.id() == audio_id && item.input_channels() > 0;
   });
   if (device == devices.end())
   {
      std::cerr << "Selected audio device has no input channel.\n";
      return 1;
   }

   pitch_to_midi processor{ *device, output };
   if (!processor.is_valid())
   {
      std::cerr << "Could not open the audio input device: " << processor.error() << '\n';
      return 1;
   }

   processor.start();
   while (running)
      q::sleep(1_s);
   processor.stop();
   processor.release_note();
   return 0;
}