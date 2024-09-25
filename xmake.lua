add_repositories("oeo-repo https://github.com/OEOTYAN/xmake-repo.git")

add_requires("oconcurrent_priority_queue v0.1.0")

target("timer")
    set_kind("headeronly")
    add_headerfiles("include/(**.h)")
    set_languages("c++20")
    add_packages("oconcurrent_priority_queue")
