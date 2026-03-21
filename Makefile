NOTES_DIR   := notes
OUT_DIR     := _build/notes
SRCS        := $(wildcard $(NOTES_DIR)/*.md)
EPUBS       := $(patsubst $(NOTES_DIR)/%.md,$(OUT_DIR)/%.epub,$(SRCS))

.PHONY: all clean notes

all: notes

notes: $(OUT_DIR)/index.html

$(OUT_DIR)/index.html: $(EPUBS)
	@echo "==> Generating notes index"
	@printf '<!DOCTYPE html>\n<html lang="en">\n<head>\n' > $@
	@printf '<meta charset="utf-8">\n' >> $@
	@printf '<meta name="viewport" content="width=device-width, initial-scale=1">\n' >> $@
	@printf '<title>Research Notes</title>\n' >> $@
	@printf '<style>\n' >> $@
	@printf 'body { font-family: system-ui, sans-serif; max-width: 40em; margin: 2em auto; padding: 0 1em; }\n' >> $@
	@printf 'h1 { border-bottom: 1px solid #ccc; padding-bottom: .3em; }\n' >> $@
	@printf 'ul { list-style: none; padding: 0; }\n' >> $@
	@printf 'li { margin: .5em 0; }\n' >> $@
	@printf 'a { color: #2c5f8a; text-decoration: none; }\n' >> $@
	@printf 'a:hover { text-decoration: underline; }\n' >> $@
	@printf '</style>\n</head>\n<body>\n' >> $@
	@printf '<h1>Research Notes</h1>\n<ul>\n' >> $@
	@for f in $(sort $(notdir $(EPUBS))); do \
		src=$(NOTES_DIR)/$$(echo "$$f" | sed 's/\.epub$$/.md/'); \
		title=$$(head -1 "$$src" | sed 's/^#\+[[:space:]]*//'); \
		printf '<li><a href="%s">%s</a></li>\n' "$$f" "$$title" >> $@; \
	done
	@printf '</ul>\n</body>\n</html>\n' >> $@

$(OUT_DIR)/%.epub: $(NOTES_DIR)/%.md | $(OUT_DIR)
	@echo "  EPUB  $<"
	@pandoc --shift-heading-level-by=-1 -o $@ $<

$(OUT_DIR):
	@mkdir -p $@

clean:
	rm -rf $(OUT_DIR)
