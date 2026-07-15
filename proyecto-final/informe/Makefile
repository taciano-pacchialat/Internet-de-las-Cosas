TEX_FILES = main.tex config.tex $(wildcard secciones/*.tex) $(wildcard figuras/*.tex)
TARGET = main

all: $(TARGET).pdf check-pages

$(TARGET).pdf: $(TEX_FILES)
	pdflatex -interaction=nonstopmode $(TARGET).tex
	pdflatex -interaction=nonstopmode $(TARGET).tex

check-pages: $(TARGET).pdf
	@echo "=== VERIFICACIÓN DE LÍMITE DE PÁGINAS ==="
	@if command -v pdfinfo >/dev/null 2>&1; then \
		PAGES=$$(pdfinfo $(TARGET).pdf | awk '/^Pages:/ {print $$2}'); \
		echo "Página(s) actual(es): $$PAGES / 10 máximo"; \
		if [ "$$PAGES" -gt 10 ]; then \
			echo "ERROR: El documento excede el límite de 10 páginas ($$PAGES > 10)."; \
			exit 1; \
		fi; \
	else \
		echo "Aviso: 'pdfinfo' no encontrado. Verifique la longitud manualmente."; \
	fi

clean:
	rm -f *.aux *.log *.out *.toc *.fls *.fdb_latexmk *.synctex.gz $(TARGET).pdf
	rm -f secciones/*.aux figuras/*.aux

.PHONY: all clean check-pages
