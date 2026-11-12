"""Banco LOCAL de rastreabilidade; nao faz consultas externas nem usa dados ficticios.

O documento da interface e EXPERIMENTAL e NAO equivale a certificado oficial MAPA.
"""
from __future__ import annotations

import json
import re
import sqlite3
from datetime import datetime
from pathlib import Path
from uuid import uuid4

DB_PATH = Path.home() / '.graovision_tcc' / 'rastreabilidade.db'
TIPOS = ('FORNECEDOR', 'CLIENTE')
PESSOA_FIELDS = (
    'razao_social', 'nome_fantasia', 'cpf_cnpj', 'inscricao_estadual',
    'telefone', 'email', 'responsavel', 'endereco', 'numero',
    'bairro', 'cidade', 'uf', 'cep', 'observacoes',
)
AMOSTRA_FIELDS = (
    'codigo_amostra', 'fornecedor_id', 'cliente_id', 'produto', 'variedade',
    'safra', 'lote', 'propriedade_origem', 'municipio_origem', 'uf_origem',
    'nota_fiscal', 'placa_veiculo', 'peso_lote_kg', 'peso_amostra_g',
    'data_recebimento', 'hora_recebimento', 'data_coleta', 'hora_coleta',
    'local_coleta', 'responsavel_coleta', 'metodo_amostragem',
    'numero_lacre', 'tipo_embalagem', 'finalidade', 'norma_referencia', 'observacoes',
)


def agora():
    return datetime.now().isoformat(timespec='seconds')


def novo_codigo():
    return f"AM-{datetime.now():%Y%m%d}-{uuid4().hex[:5].upper()}"


def _connect(db_path):
    db_path = Path(db_path)
    db_path.parent.mkdir(parents=True, exist_ok=True)
    con = sqlite3.connect(db_path, timeout=10)
    con.row_factory = sqlite3.Row
    con.execute('PRAGMA foreign_keys=ON')
    con.execute('PRAGMA busy_timeout=10000')
    return con


def _asdict(row):
    return dict(row) if row is not None else None


def _clean(value):
    return str(value or '').strip()


def _date(value, label):
    value = _clean(value)
    if not value:
        return ''
    for fmt in ('%d/%m/%Y', '%Y-%m-%d'):
        try:
            return datetime.strptime(value, fmt).strftime('%Y-%m-%d')
        except ValueError:
            pass
    raise ValueError(f'{label}: use DD/MM/AAAA')


def _time(value, label):
    value = _clean(value)
    if not value:
        return ''
    try:
        return datetime.strptime(value, '%H:%M').strftime('%H:%M')
    except ValueError:
        raise ValueError(f'{label}: use HH:MM (00:00 a 23:59)') from None


def _positive(value, label):
    value = _clean(value).replace(',', '.')
    if not value:
        return ''
    try:
        parsed = float(value)
    except ValueError:
        raise ValueError(f'{label}: informe um numero valido') from None
    if not (0 <= parsed <= 1e12):
        raise ValueError(f'{label}: valor fora da faixa permitida')
    return str(parsed)


class CadastroStore:
    def __init__(self, path=DB_PATH):
        self.path = Path(path)
        self._init_db()

    def _init_db(self):
        with _connect(self.path) as con:
            con.executescript('''
                CREATE TABLE IF NOT EXISTS pessoas (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    tipo TEXT NOT NULL CHECK (tipo IN ('FORNECEDOR','CLIENTE')),
                    razao_social TEXT NOT NULL,
                    nome_fantasia TEXT DEFAULT '', cpf_cnpj TEXT DEFAULT '',
                    inscricao_estadual TEXT DEFAULT '', telefone TEXT DEFAULT '',
                    email TEXT DEFAULT '', responsavel TEXT DEFAULT '',
                    endereco TEXT DEFAULT '', numero TEXT DEFAULT '',
                    bairro TEXT DEFAULT '', cidade TEXT DEFAULT '', uf TEXT DEFAULT '',
                    cep TEXT DEFAULT '', observacoes TEXT DEFAULT '',
                    criado_em TEXT NOT NULL, atualizado_em TEXT NOT NULL
                );
                CREATE TABLE IF NOT EXISTS amostras (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    codigo_amostra TEXT NOT NULL UNIQUE,
                    fornecedor_id INTEGER NOT NULL REFERENCES pessoas(id) ON DELETE RESTRICT,
                    cliente_id INTEGER NOT NULL REFERENCES pessoas(id) ON DELETE RESTRICT,
                    produto TEXT NOT NULL, variedade TEXT DEFAULT '', safra TEXT DEFAULT '',
                    lote TEXT NOT NULL, propriedade_origem TEXT DEFAULT '',
                    municipio_origem TEXT DEFAULT '', uf_origem TEXT DEFAULT '',
                    nota_fiscal TEXT DEFAULT '', placa_veiculo TEXT DEFAULT '',
                    peso_lote_kg TEXT DEFAULT '', peso_amostra_g TEXT DEFAULT '',
                    data_recebimento TEXT NOT NULL, hora_recebimento TEXT DEFAULT '',
                    data_coleta TEXT NOT NULL, hora_coleta TEXT DEFAULT '',
                    local_coleta TEXT DEFAULT '', responsavel_coleta TEXT NOT NULL,
                    metodo_amostragem TEXT DEFAULT '', numero_lacre TEXT DEFAULT '',
                    tipo_embalagem TEXT DEFAULT '', finalidade TEXT DEFAULT '',
                    norma_referencia TEXT DEFAULT '', observacoes TEXT DEFAULT '',
                    criado_em TEXT NOT NULL, atualizado_em TEXT NOT NULL
                );
                CREATE TABLE IF NOT EXISTS analises (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    amostra_id INTEGER REFERENCES amostras(id) ON DELETE RESTRICT,
                    sessao_id TEXT NOT NULL UNIQUE,
                    inicio TEXT NOT NULL, fim TEXT,
                    status TEXT NOT NULL, status_cnn TEXT NOT NULL,
                    resumo_json TEXT NOT NULL,
                    cadastro_snapshot_json TEXT NOT NULL,
                    registrado_em TEXT NOT NULL
                );
                CREATE INDEX IF NOT EXISTS idx_analises_amostra ON analises(amostra_id);
            ''')

    def salvar_pessoa(self, tipo, fields, id=None):
        if tipo not in TIPOS:
            raise ValueError('Tipo de cadastro invalido')
        data = {k: _clean(fields.get(k))[:500] for k in PESSOA_FIELDS}
        if not data['razao_social']:
            raise ValueError('Nome/Razao social e obrigatorio')
        if data['uf'] and (len(data['uf']) != 2 or not data['uf'].isalpha()):
            raise ValueError('UF deve ter duas letras')
        data['uf'] = data['uf'].upper()
        values = [data[k] for k in PESSOA_FIELDS]
        with _connect(self.path) as con:
            if id is None:
                cols = ','.join(PESSOA_FIELDS)
                marks = ','.join('?' for _ in PESSOA_FIELDS)
                cur = con.execute(
                    f'INSERT INTO pessoas (tipo,{cols},criado_em,atualizado_em) VALUES (?,{marks},?,?)',
                    [tipo, *values, agora(), agora()]
                )
                return cur.lastrowid
            row = self.pessoa(id)
            if row is None or row['tipo'] != tipo:
                raise ValueError('Registro nao encontrado ou tipo incompativel')
            attrs = ','.join(f'{k}=?' for k in PESSOA_FIELDS)
            con.execute(f'UPDATE pessoas SET {attrs}, atualizado_em=? WHERE id=?',
                        [*values, agora(), id])
            return id

    def pessoas(self, tipo):
        if tipo not in TIPOS:
            raise ValueError('Tipo invalido')
        with _connect(self.path) as con:
            rows = con.execute('SELECT * FROM pessoas WHERE tipo=? ORDER BY razao_social COLLATE NOCASE',
                               (tipo,)).fetchall()
            return [dict(r) for r in rows]

    def pessoa(self, id):
        with _connect(self.path) as con:
            return _asdict(con.execute('SELECT * FROM pessoas WHERE id=?',(int(id),)).fetchone())

    def salvar_amostra(self, fields, id=None):
        data = {k: _clean(fields.get(k))[:600] for k in AMOSTRA_FIELDS}
        for key in ('codigo_amostra','produto','lote','responsavel_coleta'):
            if not data[key]:
                raise ValueError(f'Campo obrigatorio: {key.replace("_", " ")}')
        data['codigo_amostra'] = data['codigo_amostra'].upper()
        for k in ('fornecedor_id','cliente_id'):
            try: data[k] = int(data[k])
            except (TypeError,ValueError): raise ValueError('Selecione fornecedor e cliente cadastrados') from None
        for k in ('data_recebimento','data_coleta'):
            data[k] = _date(data[k], k.replace('_', ' '))
            if not data[k]: raise ValueError(f'{k.replace("_", " ")} e obrigatoria')
        for k in ('hora_recebimento','hora_coleta'):
            data[k] = _time(data[k], k.replace('_', ' '))
        for k in ('peso_lote_kg','peso_amostra_g'):
            data[k] = _positive(data[k], k.replace('_', ' '))
        for k in ('uf_origem',):
            if data[k] and (len(data[k]) != 2 or not data[k].isalpha()):
                raise ValueError('UF da origem deve ter duas letras')
            data[k] = data[k].upper()
        forn,cli = self.pessoa(data['fornecedor_id']),self.pessoa(data['cliente_id'])
        if not forn or forn['tipo']!='FORNECEDOR' or not cli or cli['tipo']!='CLIENTE':
            raise ValueError('Fornecedor ou cliente nao encontrado no cadastro')
        with _connect(self.path) as con:
            values = [data[k] for k in AMOSTRA_FIELDS]
            if id is None:
                columns=','.join(AMOSTRA_FIELDS)
                slots=','.join('?' for _ in AMOSTRA_FIELDS)
                try:
                    cur=con.execute(f'INSERT INTO amostras ({columns},criado_em,atualizado_em) VALUES ({slots},?,?)',
                                    [*values,agora(),agora()])
                    return cur.lastrowid
                except sqlite3.IntegrityError as exc:
                    raise ValueError('Codigo da amostra duplicado') from exc
            if not self.amostra(id):
                raise ValueError('Amostra nao encontrada')
            assignments=','.join(f'{key}=?' for key in AMOSTRA_FIELDS)
            try:
                con.execute(f'UPDATE amostras SET {assignments}, atualizado_em=? WHERE id=?',
                            [*values,agora(),id])
                return id
            except sqlite3.IntegrityError as exc:
                raise ValueError('Codigo da amostra duplicado') from exc

    def amostra(self, id):
        with _connect(self.path) as con:
            return _asdict(con.execute('SELECT * FROM amostras WHERE id=?',(int(id),)).fetchone())

    def amostras(self):
        with _connect(self.path) as con:
            rows=con.execute('''SELECT a.*,f.razao_social AS fornecedor, c.razao_social AS cliente
                FROM amostras a JOIN pessoas f ON a.fornecedor_id=f.id
                JOIN pessoas c ON a.cliente_id=c.id
                ORDER BY a.criado_em DESC, a.id DESC''').fetchall()
            return [dict(r) for r in rows]

    def snapshot(self, amostra_id):
        a=self.amostra(amostra_id)
        if not a: raise ValueError('Amostra nao encontrada')
        return {'amostra':a,
                'fornecedor':self.pessoa(a['fornecedor_id']),
                'cliente':self.pessoa(a['cliente_id'])}

    def registrar_analise(self, amostra_id, session, snapshot_override=None):
        """Registra snapshot do cadastro no momento da analise para rastreabilidade."""
        sample=session.sample
        sample_id=sample.id
        resumo={
            'sessao_id':sample_id,
            'inicio':session.data_hora_inicio.isoformat(timespec='seconds'),
            'fim':session.data_hora_fim.isoformat(timespec='seconds') if session.data_hora_fim else '',
            'status':sample.status_final, 'motivo':sample.motivo_final,
            'cnn_status':sample.status_cnn, 'cnn_motivo':sample.motivo_cnn,
            'deteccoes_validas':sample.total_graos,'deteccoes_bad':sample.graos_ruins,
            'percentual_bad':sample.percentual_ruins,
            'P1':sample.peso_inicial, 'P2':sample.peso_referencia,
            'PF':sample.peso_final,'P3':sample.peso_retorno,
            'P4':sample.peso_segunda_subida,'P5':sample.peso_segundo_retorno,
            'temp_camara':sample.temperatura_camara,
            'umidade':sample.umidade_final,
            'metodo_umidade':sample.metodo_umidade,
            'aquecimento_comandado':bool(sample.aquecimento_comandado),
            'eventos_aquecedor':sample.eventos_aquecedor,
            'codigo_erro':sample.codigo_erro_arduino,
            'repetibilidade_centro':sample.repetibilidade_centro_g,
            'repetibilidade_topo':sample.repetibilidade_topo_g,
            'imagem_anotada':next((item.get('imagem_anotada') for item in reversed(sample.roi_registros)
                                   if isinstance(item,dict) and item.get('imagem_anotada')),None),
        }
        snapshot=(snapshot_override if snapshot_override is not None else
                  (self.snapshot(amostra_id) if amostra_id is not None else {}))
        with _connect(self.path) as con:
            con.execute('''INSERT INTO analises
                (amostra_id,sessao_id,inicio,fim,status,status_cnn,resumo_json,cadastro_snapshot_json,registrado_em)
                VALUES (?,?,?,?,?,?,?,?,?)''',(
                amostra_id,sample_id,resumo['inicio'],resumo['fim'],resumo['status'],
                resumo['cnn_status'],json.dumps(resumo,ensure_ascii=False,default=str),
                json.dumps(snapshot,ensure_ascii=False),agora()))
            aid=con.execute('SELECT last_insert_rowid()').fetchone()[0]
            return aid

    def analises(self):
        with _connect(self.path) as con:
            rows=con.execute('''SELECT n.id,n.sessao_id,n.inicio,n.status,n.status_cnn,
                COALESCE(a.codigo_amostra,'SEM CADASTRO') AS codigo_amostra,
                COALESCE(f.razao_social,'') AS fornecedor,
                COALESCE(c.razao_social,'') AS cliente
                FROM analises n LEFT JOIN amostras a ON n.amostra_id=a.id
                LEFT JOIN pessoas f ON a.fornecedor_id=f.id
                LEFT JOIN pessoas c ON a.cliente_id=c.id
                ORDER BY n.id DESC''').fetchall()
            return [dict(r) for r in rows]

    def analise(self, id):
        with _connect(self.path) as con:
            row=con.execute('SELECT * FROM analises WHERE id=?',(int(id),)).fetchone()
        if not row:return None
        result=dict(row)
        result['resumo']=json.loads(result['resumo_json'])
        result['cadastro_snapshot']=json.loads(result['cadastro_snapshot_json'])
        return result


def criar_backup(store, destino):
    """Backup SQLite consistente para arquivo local; nunca envia dados para internet."""
    destino=Path(destino)
    destino.parent.mkdir(parents=True, exist_ok=True)
    if destino.resolve()==store.path.resolve():
        raise ValueError('Backup deve ser salvo em arquivo diferente do banco atual')
    with _connect(store.path) as origem:
        with sqlite3.connect(destino) as copia:
            origem.backup(copia)
    return destino
