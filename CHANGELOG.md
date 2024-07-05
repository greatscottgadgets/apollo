# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).


## [1.0.5] - 2024-07-04

### Added
- Add example udev rules file.

### Changed
- Update README.
- Use pid.codes test VID/PID for non-Cynthion boards.

### Fixed
- Ignore devices not running Apollo.

### Removed
- Remove `flash` alias for `flash-program`.


## [1.0.4] - 2024-06-26

### Added

- Show both serial number and flash UID in `apollo info` and `apollo
  flash-info`.
- Add changelog.
- Add command `apollo flash --fast`.
- Add support for Raspberry Pi Pico RP2040.
- Add support for hardware revision detection on Cynthion >=r0.6.

### Changed

- Do not trigger reconfiguration after `apollo flash --fast`.
- Select USB string descriptors at run time.
- Avoid automatic FPGA takeover for info commands.
- Generate rising edges instead of UART output in ApolloAdvertiser.
- Use weak functions for default firmware behavior.

### Deprecated

- Deprecate command `apollo flash-fast`.

### Fixed

- Update incorrect or confusing usage of "LUNA".


## [1.0.3] - 2024-05-29

### Changed

- Bump USB API to 1.1.

### Fixed

- Hold Sideband PHY RESET low on older Cynthions, fixing Sideband port usage
  for hardware revision r0.3 through r0.5.
- Fix `fpga_requesting_port()` indication glitch at start-up.
- Correct Base32 representation of serial number.


## [1.0.2] - 2024-05-22

### Changed

- Replace advertisement UART reception with edge counter in firmware.


## [1.0.1] - 2024-05-21

### Fixed

- Fix handing off USB port to Apollo under Windows.
- Allow FPGA USB by default.


## [1.0.0] - 2024-05-18

### Added

- Initial release.


[1.0.5]: https://github.com/greatscottgadgets/apollo/compare/v1.0.4...v1.0.5
[1.0.4]: https://github.com/greatscottgadgets/apollo/compare/v1.0.3...v1.0.4
[1.0.3]: https://github.com/greatscottgadgets/apollo/compare/v1.0.2...v1.0.3
[1.0.2]: https://github.com/greatscottgadgets/apollo/compare/v1.0.1...v1.0.2
[1.0.1]: https://github.com/greatscottgadgets/apollo/compare/v1.0.0...v1.0.1
[1.0.0]: https://github.com/greatscottgadgets/apollo/releases/tag/v1.0.0
