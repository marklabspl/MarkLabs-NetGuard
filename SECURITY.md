# Security policy

## Supported versions

Only the newest Master and Power Module versions in the active `firmware`
directories are supported.

## Deployment boundary

NetGuard is designed for a trusted management LAN or a dedicated management
VLAN. Do not expose its HTTP interface, MQTT broker connection or UART directly
to the public Internet. Use a VPN or a TLS-terminating management gateway for
remote access.

Change the initial `admin` password before enabling configuration changes,
manual RESET or OTA. Use a dedicated MQTT account restricted to the configured
NetGuard topics.

Current limitations:

- the embedded web interface uses HTTP Basic Authentication over HTTP;
- MQTT TLS and HTTPS diagnostic probes do not validate the peer certificate;
- OTA validates the ESP32 image and product filename but does not yet require a
  cryptographic firmware signature.

These limitations are not acceptable on an untrusted or shared network.

## Reporting

Report vulnerabilities privately to MarkLabs.pl. Do not include credentials,
private keys, production configuration backups or customer network details in a
public issue.
