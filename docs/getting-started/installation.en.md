# Install and Update

Flori Input runs on Windows. Installer and portable packages are available from
the [project Releases](https://github.com/flowersauce/Flori-Input/releases). Check the publisher and filename before
downloading, and avoid untrusted mirrors.

## Installer

Download `Flori-Input-v<version>-windows-x64-setup.exe` and follow the installer prompts. Before updating, stop any
active input task and close the app.

## Portable package

Download `Flori-Input-v<version>-windows-x64-portable.zip`, extract it to a writable directory, then run
`Flori-Input.exe`. Do not run the app directly from the ZIP. The portable package stores settings in
`config/config.jsonc` next to the app and event scripts in the neighboring `scripts` directory. You can move both
directories with the portable app.

## Where settings and scripts are stored

The installer stores settings and scripts in the installation root, alongside the `current` version directory:

```text
<installation root>\
├─ current\Flori-Input.exe
├─ config\config.jsonc
└─ scripts\
```

The actual installation location depends on how the app was installed. To find it, click **Open scripts folder** on the
**Script** page. Its parent is the installation root; `config\config.jsonc` is alongside `scripts`. Because `config` and
`scripts` are outside `current`, an installer update does not replace them with the version directory. Keep copies of
both directories when backing up your settings and scripts.
