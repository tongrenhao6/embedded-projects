# -*- mode: python ; coding: utf-8 -*-


a = Analysis(
    ['beehive_gateway/launcher.py'],
    pathex=[],
    binaries=[],
    datas=[('beehive_gateway/web', 'beehive_gateway/web')],
    hiddenimports=['beehive_gateway.api', 'mysql.connector.locales.eng.client_error'],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=['PyQt5', 'PySide6'],
    noarchive=False,
    optimize=0,
)
pyz = PYZ(a.pure)

exe = EXE(
    pyz,
    a.scripts,
    a.binaries,
    a.datas,
    [],
    name='BeehiveMonitor',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    upx_exclude=[],
    runtime_tmpdir=None,
    console=True,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
)
