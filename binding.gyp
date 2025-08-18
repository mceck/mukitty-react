{
    "targets": [
        {
            "target_name": "mukitty",
            "sources": ["mukitty.c", "microui.c"],
            "conditions": [
                ['OS=="linux"', {}],
                ['OS=="mac"', {}],
            ],
        }
    ]
}
