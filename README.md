# Vulkan Renderer

This project is meant to polish a few skills of mine while learning new things I've never worked on before. This document will try to explain why this project is relevant, what it tries to achieve and how it does it.

## Why?

- **Writing modern C++**: Since most of my past projects have only used the old paradigm of "C with classes" related to C++98 this is a good chance to start using the more modern C++11 onwards, with features even on newer standards like C++14 and C++17. Learning these features and the new recommended practices has proven to be essential to be a capable programmer in the games industry.
- **Working with a modern API** and **Working with graphics**: Vulkan is a modern and complex API. This is a good exercise to show how I can work with APIs that are not very easi to do while I also learn more about working with computer generated APIs. Vulkan forces you to learn about how graphics work in a way that older APIs like OpenGL or DirectX (before version 12) don't, this is because of it's extreme low overhead and transparent model that makes you work very close to the metal, giving you performance and control at the cost of complexity.
- **Bridging the gap**: Making a project meant for rendering models, animations, sprites and scenes in different ways also helps understanding how artist-generated work gets put into a game. Since I have a decent idea about how artists work and I've done some things myself (although I would never claim to be an artist) this is a very good way to bridge the gap between the two disciplines, something very important given my aspirations to be a game programmer/engineer in a big studio because knowing how other disciplines work makes work that much smoother.

## How?

I'm following the iso C++ Core Guidelines available in [this repository](https://github.com/isocpp/CppCoreGuidelines) and the recommendations for modern C++ by its creator Bjarne StroustrupHerb and Herb Sutter published in their books and explained in talks such as the ones given in the [cppcon](https://cppcon.org/).

The things that have been implemented so far are the following:

* Created a window and a vulkan instance with custom layers and extensions in a safe way.
* Configured the Debug Report Extension to get runtime debug information about the application.
* Implemented visualization, processing and selection of physical devices available on the system that support vulkan.
* Implemented visualization, processing, selection and creating of necesssary queues and logical devices. 



