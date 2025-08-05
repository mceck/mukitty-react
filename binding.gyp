{
    "targets": [
        {
            "target_name": "mukitty",
            "sources": ["mukitty.cpp", "microui.c"],
            "conditions": [
                ['OS=="linux"', {}],
                [
                    'OS=="mac"',
                    {
                        "xcode_settings": {"OTHER_LDFLAGS": ["-Wl,-rpath"]},
                    },
                ],
            ],
        }
    ]
}
