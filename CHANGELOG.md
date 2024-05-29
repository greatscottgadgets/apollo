# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).


## [Unreleased]
### Changed
- Hold Sideband PHY RESET low on older Cynthions
- Bumped USB API to 1.1
### Fixed
- fix `fpga_requesting_port()` glitch
- Correct Base32 representation of serial number


## [1.0.2] - 2024-05-22
### Changed
- firmware.fpga_adv: replace UART reception with edge counter


## [1.0.1] - 2024-05-21
### Fixed
- Fix handing off USB port to Apollo under Windows
- Allow FPGA USB by default


## [1.0.0] - 2024-05-18
### Added
- Initial release


[Unreleased]: https://github.com/greatscottgadgets/apollo/compare/v1.0.2...HEAD
[1.0.2]: https://github.com/greatscottgadgets/apollo/compare/v1.0.1...v1.0.2
[1.0.1]: https://github.com/greatscottgadgets/apollo/compare/v1.0.0...v1.0.1
[1.0.0]: https://github.com/greatscottgadgets/apollo/releases/tag/v1.0.0
