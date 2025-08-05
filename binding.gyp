{
    "targets": [
        {
            "target_name": "mukitty",
            "sources": ["mukitty.cpp", "microui.c"],
            "conditions": [
                ['OS=="linux"', {}],
                ['OS=="mac"', {}],
            ],
        }
    ]
}
