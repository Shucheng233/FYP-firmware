idf_component_register(
    SRCS
        "app_main.c"

        "audio/audio_input.c"
        "audio/audio_output.c"

        "drivers/es8311.c"
        "drivers/i2s_driver.c"

        "network/stt_client.c"
        "network/tts_client.c"

        "pipeline/voice_pipeline.c"

    INCLUDE_DIRS
        "."
        "audio"
        "drivers"
        "network"
        "pipeline"
)