# Análise Semântica - Relatório de Implementação

## Resumo
Foram integradas funcionalidades de análise semântica ao analisador sintático do compilador C99.
A análise opera simultaneamente com a sintática, detectando erros semânticos durante a geração da AST.

## Aspectos Implementados

### 1. **Verificação de Declaração de Variáveis**
   - ✓ Rastreamento de variáveis declaradas em cada escopo
   - ✓ Detecção de redeclaração de variáveis no mesmo escopo
   - ✓ Busca em escopos aninhados (escopo local → escopo da função → escopo global)
   - ✓ Erro ao usar variáveis não declaradas

**Exemplo de Erro:**
```c
int main(){
    x = 10;  // Erro: 'x' não declarada
    int x;
    return 0;
}
```

### 2. **Verificação de Inicialização**
   - ✓ Rastreamento de variáveis inicializadas vs não inicializadas
   - ✓ Marcação automática de inicialização em:
     - Declarações com inicializador: `int x = 10;`
     - Primeiras atribuições: `x = 20;`
     - Parâmetros de função (sempre inicializados)
   - ✓ Erro ao usar variáveis sem inicialização
   - ✓ Atribuição em incrementos/decrementos marca como inicializada

**Exemplo de Erro:**
```c
int main(){
    int x;
    int y = x + 1;  // Erro: 'x' usado sem inicialização
    return 0;
}
```

### 3. **Rastreamento de Uso de Variáveis**
   - ✓ Marcação de variáveis quando utilizadas em expressões
   - ✓ Aviso para variáveis declaradas mas nunca utilizadas
   - ✓ Distinção entre declaração, inicialização e uso

**Exemplo de Aviso:**
```c
int main(){
    int x = 10;  // Aviso: variável não usada
    return 0;
}
```

## Funcionalidades de Escopo

### Gerenciamento de Escopos
- **Função**: Cada função cria um novo escopo
- **Bloco**: Cada bloco `{}` cria um novo escopo aninhado
- **Herança**: Escopos filhos podem acessar símbolos de escopos pais
- **Isolamento**: Variáveis locais ocultam variáveis de escopos exteriores

**Estrutura:**
```
Global Scope
├── Escopo da Função 1
│   ├── Escopo do Bloco if
│   ├── Escopo do Bloco for
│   └── ...
├── Escopo da Função 2
│   └── ...
└── ...
```

### Verificação de Tipos
- ✓ Tipos suportados: `int`, `char`, `void`
- ✓ Inferência de tipos em expressões
- ✓ Validação de compatibilidade em atribuições
- ✓ Detecção de incompatibilidade de tipos (char ≠ string, void ≠ outros)

## Integração com o Parser

### Pontos de Verificação Semântica

1. **Declarações** (`analisar_declaracao`):
   - Verifica redeclaração no escopo local
   - Registra variável com tipo
   - Marca inicialização se houver expressão inicial

2. **Identificadores** (`analisar_fator`):
   - Verifica se variável está declarada
   - Marca como "usada"
   - Emite erro se não declarada
   - Emite erro se usada sem inicialização

3. **Atribuições** (`analisar_atribuicao`, `analisar_comando_iniciado_por_id`):
   - Verifica se variável existe
   - Marca como inicializada
   - Valida tipo da expressão
   - Emite erro se tipo incompatível

4. **Funções** (`analisar_funcao`):
   - Cria novo escopo para parâmetros e corpo
   - Registra parâmetros como variáveis inicializadas
   - Verifica uso de variáveis dentro do corpo

5. **Blocos** (`analisar_bloco`):
   - Cria novo escopo aninhado
   - Fecha escopo ao sair do bloco
   - Emite avisos de variáveis não utilizadas

6. **Incremento/Decremento** (`analisar_incremento_decremento`, `analisar_expressao_de_incremento`):
   - Verifica declaração da variável
   - Marca como inicializada e usada

## Mensagens de Erro/Aviso

### Erros Semânticos
- `"Variável '%s' não declarada"` - Uso de variável não declarada
- `"Variável '%s' já declarada neste escopo"` - Redeclaração
- `"Uso de variável '%s' sem inicialização"` - Uso sem inicializar
- `"Tipo incompatível: não é possível atribuir string a '%s'"` - Atribuição inválida
- `"Atribuição inválida: variável do tipo void"` - Atribuição em void

### Avisos
- `"[ AVISO ] Variável não usada '%s' declarada em linha X, coluna Y"` - Variável declarada mas não utilizada

## Exemplos de Uso

### Teste 1: Variáveis não declaradas
```c
int main(){
    x = 10;      // Erro: 'x' não declarada
    int y;
    return 0;
}
```
**Resultado:** `[ ERRO SEMÂNTICO ] Variável 'x' não declarada`

### Teste 2: Uso sem inicialização
```c
int main(){
    int x;
    int y = x + 5;  // Erro: 'x' sem inicialização
    return 0;
}
```
**Resultado:** `[ ERRO SEMÂNTICO ] Uso de variável 'x' sem inicialização`

### Teste 3: Variáveis não utilizadas
```c
int main(){
    int x = 10;
    int y = 20;     // Aviso: não utilizada
    return x;
}
```
**Resultado:** `[ AVISO ] Variável não usada "y" declarada em linha 3, coluna 9`

### Teste 4: Escopos aninhados
```c
int main(){
    int x = 10;
    {
        int x = 20;  // Variável local no bloco
        x = x + 5;
    }
    return x;        // Referencia o x do main
}
```
**Resultado:** Aceito (escopos trabalham corretamente)

## Notas Técnicas

- A análise semântica **não interrompe** a análise sintática
- Erros semânticos são registrados mas o programa continua analisando
- Avisos de variáveis não utilizadas aparecem após fechamento de escopo
- Símbolos são armazenados em lista encadeada para rápida busca
- Escopos são hierárquicos permitindo busca em múltiplos níveis

## Compatibilidade com Código Existente

A implementação foi integrada ao código existente sem modificações estruturais significativas:
- Mantém todas as funcionalidades sintáticas
- Adiciona análise semântica parallel
- Não afeta a geração de AST
- Compatível com tratamento de erros existente

## Trabalho Futuro

Possíveis extensões:
- ✓ Análise de constantes
- ✓ Verificação de tipos em retorno de funções
- ✓ Análise de chamadas de função (parâmetros vs argumentos)
- ✓ Detecção de variáveis possivelmente não inicializadas
- ✓ Suporte a arrays e estruturas
